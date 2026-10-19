#pragma once

// starts a loopback-only http server that lets panorama ask the gc to measure
// the latency to a list of game servers (source query udp round trip)
//
//   GET http://127.0.0.1:<port>/ping?addrs=ip:port,ip:port,...
//   -> {"ip:port": <ms or -1>, ...}
//
//   GET http://127.0.0.1:<port>/health
//   -> {"ok":true}
void PingServiceStart();
