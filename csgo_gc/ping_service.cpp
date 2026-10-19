#include "stdafx.h"
#include "ping_service.h"
#include "config.h"

#include <atomic>
#include <cctype>
#include <cstring>
#include <chrono>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>

using SocketHandle = SOCKET;
constexpr SocketHandle InvalidSocket = INVALID_SOCKET;

static void CloseSocket(SocketHandle s) { closesocket(s); }
static int PollSockets(pollfd *fds, size_t count, int timeoutMs)
{
    return WSAPoll(fds, static_cast<ULONG>(count), timeoutMs);
}
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

using SocketHandle = int;
constexpr SocketHandle InvalidSocket = -1;

static void CloseSocket(SocketHandle s) { close(s); }
static int PollSockets(pollfd *fds, size_t count, int timeoutMs)
{
    return poll(fds, static_cast<nfds_t>(count), timeoutMs);
}
#endif

namespace
{

using Clock = std::chrono::steady_clock;

constexpr size_t MaxTargets = 64;
constexpr size_t MaxRequestSize = 8192;
constexpr int PingRounds = 2;
constexpr int PingTimeoutMs = 800;

struct Target
{
    std::string name; // the "ip:port" string as received
    sockaddr_in addr{};
    int bestMs{ -1 };
    int players{ -1 };
    int bots{ -1 };
};

std::string UrlDecode(std::string_view in)
{
    std::string out;
    out.reserve(in.size());

    for (size_t i = 0; i < in.size(); i++)
    {
        if (in[i] == '%' && i + 2 < in.size() && std::isxdigit(static_cast<unsigned char>(in[i + 1])) &&
            std::isxdigit(static_cast<unsigned char>(in[i + 2])))
        {
            out.push_back(static_cast<char>(std::stoi(std::string(in.substr(i + 1, 2)), nullptr, 16)));
            i += 2;
        }
        else
        {
            out.push_back(in[i]);
        }
    }

    return out;
}

bool ParseTarget(const std::string &text, Target &target)
{
    size_t colon = text.rfind(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= text.size())
    {
        return false;
    }

    std::string ip = text.substr(0, colon);
    unsigned port = 0;
    const char *portBegin = text.data() + colon + 1;
    const char *portEnd = text.data() + text.size();
    auto [ptr, ec] = std::from_chars(portBegin, portEnd, port);
    if (ec != std::errc{} || ptr != portEnd || port == 0 || port > 65535)
    {
        return false;
    }

    target.addr = {};
    target.addr.sin_family = AF_INET;
    target.addr.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, ip.c_str(), &target.addr.sin_addr) != 1)
    {
        return false;
    }

    target.name = text;
    return true;
}

// one round: fire an A2S_INFO at every target and record the time of the first reply from each
void PingRound(std::vector<Target> &targets)
{
    // source query: ff ff ff ff 'T' "Source Engine Query\0"
    static const char query[] = "\xff\xff\xff\xffTSource Engine Query";

    std::vector<SocketHandle> sockets(targets.size(), InvalidSocket);
    std::vector<Clock::time_point> sent(targets.size());
    std::vector<bool> done(targets.size(), false);

    for (size_t i = 0; i < targets.size(); i++)
    {
        SocketHandle s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == InvalidSocket)
        {
            done[i] = true;
            continue;
        }

        sockets[i] = s;
        sent[i] = Clock::now();
        if (sendto(s, query, sizeof(query), 0, reinterpret_cast<const sockaddr *>(&targets[i].addr),
            sizeof(sockaddr_in)) < 0)
        {
            done[i] = true;
        }
    }

    const auto deadline = Clock::now() + std::chrono::milliseconds(PingTimeoutMs);

    for (;;)
    {
        int remaining = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count());
        if (remaining <= 0)
        {
            break;
        }

        std::vector<pollfd> fds;
        std::vector<size_t> indices;
        for (size_t i = 0; i < targets.size(); i++)
        {
            if (!done[i] && sockets[i] != InvalidSocket)
            {
                pollfd fd{};
                fd.fd = sockets[i];
                fd.events = POLLIN;
                fds.push_back(fd);
                indices.push_back(i);
            }
        }

        if (fds.empty() || PollSockets(fds.data(), fds.size(), remaining) <= 0)
        {
            break;
        }

        const auto now = Clock::now();
        for (size_t f = 0; f < fds.size(); f++)
        {
            if (!(fds[f].revents & (POLLIN | POLLERR | POLLHUP)))
            {
                continue;
            }

            size_t i = indices[f];
            char buffer[1400];
            sockaddr_in from{};
            socklen_t fromLen = sizeof(from);
            int received = static_cast<int>(recvfrom(sockets[i], buffer, sizeof(buffer), 0,
                reinterpret_cast<sockaddr *>(&from), &fromLen));
            done[i] = true;

            // must be a source engine reply (0xffffffff header) from the address we pinged
            if (received >= 5 && from.sin_addr.s_addr == targets[i].addr.sin_addr.s_addr &&
                from.sin_port == targets[i].addr.sin_port &&
                static_cast<uint8_t>(buffer[0]) == 0xff && static_cast<uint8_t>(buffer[1]) == 0xff &&
                static_cast<uint8_t>(buffer[2]) == 0xff && static_cast<uint8_t>(buffer[3]) == 0xff)
            {
                int ms = static_cast<int>(
                    std::chrono::duration_cast<std::chrono::microseconds>(now - sent[i]).count() / 1000);
                if (targets[i].bestMs < 0 || ms < targets[i].bestMs)
                {
                    targets[i].bestMs = ms;
                }

                uint8_t type = static_cast<uint8_t>(buffer[4]);
                if (type == 'A' && received >= 9)
                {
                    // challenge: resend the query with it appended and wait for the real info reply
                    char request[sizeof(query) + 4];
                    memcpy(request, query, sizeof(query));
                    memcpy(request + sizeof(query), buffer + 5, 4);
                    if (sendto(sockets[i], request, sizeof(request), 0,
                        reinterpret_cast<const sockaddr *>(&targets[i].addr), sizeof(sockaddr_in)) >= 0)
                    {
                        done[i] = false;
                    }
                }
                else if (type == 'I')
                {
                    // protocol byte, then name, map, folder, game strings, appid, players, max, bots
                    int p = 6;
                    for (int s = 0; s < 4 && p < received; s++)
                    {
                        while (p < received && buffer[p] != '\0')
                        {
                            p++;
                        }
                        p++;
                    }
                    p += 2;
                    if (p + 3 <= received)
                    {
                        targets[i].players = static_cast<uint8_t>(buffer[p]);
                        targets[i].bots = static_cast<uint8_t>(buffer[p + 2]);
                    }
                }
            }
        }
    }

    for (SocketHandle s : sockets)
    {
        if (s != InvalidSocket)
        {
            CloseSocket(s);
        }
    }
}

std::string HandlePing(std::string_view query)
{
    std::vector<Target> targets;

    size_t pos = 0;
    while (pos < query.size())
    {
        size_t amp = query.find('&', pos);
        std::string_view pair = query.substr(pos, amp == std::string_view::npos ? amp : amp - pos);
        pos = amp == std::string_view::npos ? query.size() : amp + 1;

        if (!pair.starts_with("addrs="))
        {
            continue;
        }

        std::string list = UrlDecode(pair.substr(6));
        size_t start = 0;
        while (start <= list.size() && targets.size() < MaxTargets)
        {
            size_t comma = list.find(',', start);
            std::string item = list.substr(start, comma == std::string::npos ? comma : comma - start);
            start = comma == std::string::npos ? list.size() + 1 : comma + 1;

            Target target;
            if (ParseTarget(item, target))
            {
                targets.push_back(std::move(target));
            }
        }
    }

    for (int round = 0; round < PingRounds && !targets.empty(); round++)
    {
        PingRound(targets);
    }

    std::string json = "{";
    for (size_t i = 0; i < targets.size(); i++)
    {
        if (i)
        {
            json += ',';
        }
        json += std::format("\"{}\":{{\"ms\":{},\"players\":{},\"bots\":{}}}", targets[i].name,
            targets[i].bestMs, targets[i].players, targets[i].bots);
    }
    json += '}';
    return json;
}

void SendResponse(SocketHandle client, int status, const char *statusText, const std::string &body)
{
    std::string response = std::format(
        "HTTP/1.1 {} {}\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: {}\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Cache-Control: no-store\r\n"
        "Connection: close\r\n\r\n{}",
        status, statusText, body.size(), body);

    send(client, response.data(), static_cast<int>(response.size()), 0);
}

void HandleClient(SocketHandle client, uint16_t port)
{
    std::string request;
    char buffer[1024];
    while (request.find("\r\n\r\n") == std::string::npos && request.size() < MaxRequestSize)
    {
        pollfd fd{};
        fd.fd = client;
        fd.events = POLLIN;
        if (PollSockets(&fd, 1, 2000) <= 0)
        {
            return;
        }

        int received = static_cast<int>(recv(client, buffer, sizeof(buffer), 0));
        if (received <= 0)
        {
            return;
        }

        request.append(buffer, received);
    }

    size_t lineEnd = request.find("\r\n");
    if (lineEnd == std::string::npos)
    {
        SendResponse(client, 400, "Bad Request", "{}");
        return;
    }

    // refuse requests that were not addressed to loopback (dns rebinding from a browser)
    std::string lower = request;
    for (char &c : lower)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    const std::string hostLocal = std::format("\r\nhost: 127.0.0.1:{}\r\n", port);
    const std::string hostName = std::format("\r\nhost: localhost:{}\r\n", port);
    if (lower.find(hostLocal) == std::string::npos && lower.find(hostName) == std::string::npos)
    {
        SendResponse(client, 403, "Forbidden", "{}");
        return;
    }

    std::string_view line{ request.data(), lineEnd };
    if (!line.starts_with("GET "))
    {
        SendResponse(client, 405, "Method Not Allowed", "{}");
        return;
    }

    line.remove_prefix(4);
    line = line.substr(0, line.find(' '));

    std::string_view path = line.substr(0, line.find('?'));
    std::string_view query = path.size() < line.size() ? line.substr(path.size() + 1) : std::string_view{};

    if (path == "/health")
    {
        SendResponse(client, 200, "OK", "{\"ok\":true}");
    }
    else if (path == "/ping")
    {
        SendResponse(client, 200, "OK", HandlePing(query));
    }
    else
    {
        SendResponse(client, 404, "Not Found", "{}");
    }
}

void ServerThread(SocketHandle listener, uint16_t port)
{
    for (;;)
    {
        SocketHandle client = accept(listener, nullptr, nullptr);
        if (client == InvalidSocket)
        {
            continue;
        }

        HandleClient(client, port);
        CloseSocket(client);
    }
}

} // namespace

void PingServiceStart()
{
    const int configPort = GetConfig().m_pingServicePort;
    if (configPort <= 0 || configPort > 65535)
    {
        return;
    }

    const uint16_t port = static_cast<uint16_t>(configPort);

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        Platform::Print("Ping service: WSAStartup failed\n");
        return;
    }
#endif

    SocketHandle listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == InvalidSocket)
    {
        Platform::Print("Ping service: socket creation failed\n");
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (bind(listener, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0 || listen(listener, 8) != 0)
    {
        Platform::Print("Ping service: could not listen on 127.0.0.1:{}\n", port);
        CloseSocket(listener);
        return;
    }

    std::thread(ServerThread, listener, port).detach();
    Platform::Print("Ping service listening on 127.0.0.1:{}\n", port);
}
