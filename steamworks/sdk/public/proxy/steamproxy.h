// jesus fucking christ
#ifndef STEAMPROXY_H
#define STEAMPROXY_H

template<class T>
struct TypeIdentity
{
    using type = T;
};

template<class T>
using TypeIdentityT = typename TypeIdentity<T>::type;

template<class Ret, class... Args>
struct Orig
{
    void *self;
    Ret (*fn)(void *, Args...);
    Ret operator()(Args... args) const { return fn(self, args...); }
};

template<class Iface, auto Pmf, class Ret, class... Args>
Ret DoMakeOrig(void *self, Args... args)
{
    return (static_cast<Iface *>(self)->*Pmf)(args...);
}

template<auto Pmf>
struct OrigOf;

template<class I, class R, class... A, R (I::*P)(A...)>
struct OrigOf<P>
{
    using type = Orig<R, A...>;
    static type get(void *s) { return type{ s, &DoMakeOrig<I, P, R, A...> }; }
};

template<class I, class R, class... A, R (I::*P)(A...) const>
struct OrigOf<P>
{
    using type = Orig<R, A...>;
    static type get(void *s) { return type{ s, &DoMakeOrig<I, P, R, A...> }; }
};

template<auto Pmf>
auto MakeOrig(void *s) { return OrigOf<Pmf>::get(s); }

#endif // STEAMPROXY_H
