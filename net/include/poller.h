#pragma once

#include <vector>
#include <map>
#include <sys/epoll.h>

#include "timestamp.h"
#include "enum.h"
#include "channel.h"
#include "eventloop.h"

namespace net
{

    class Poller
    {
    public:
        using ChannelList = std::vector<Channel *>;

    public:
        Poller(EventLoop *loop) : _loop(loop) {}
        virtual ~Poller() = default;
        virtual Timestamp poll(int timeoutMS, ChannelList *activechannels) = 0;
        virtual void updateChannel(Channel *channel) = 0;
        virtual void removeChannel(Channel *channel) = 0;
        virtual bool hasChannel(Channel *channel) const;
        static Poller *newDefaultPoller(EventLoop *loop);

    protected:
        using ChannelMap = std::map<int, Channel *>;
        ChannelMap _channels;

    private:
        EventLoop *_loop;
    };

    class EPollPoller : public Poller
    {

    public:
        EPollPoller(EventLoop *loop);
        ~EPollPoller() override;
        Timestamp poll(int timeoutMS, ChannelList *activechannels) override;
        void updateChannel(Channel *channel) override;
        void removeChannel(Channel *channel) override;

    private:
        void fillActiveChannels(int numEvents, ChannelList *activechannels);
        void update(int operation, Channel *channel);

    private:
        using EventList = std::vector<epoll_event>;
        EventList _events;
        int _epollfd;
    };

} // namespace net
