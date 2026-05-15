#include "poller.h"
#include <assert.h>
namespace net
{
    bool Poller::hasChannel(Channel *channel) const
    {
        return _channels.find(channel->fd()) != _channels.end();
    }

    Poller *Poller::newDefaultPoller(EventLoop *loop)
    {
        return new EPollPoller(loop);
    }

    int creatfd()
    {
        int fd = epoll_create1(EPOLL_CLOEXEC);
        if (fd < 0)
        {
            LOG_ERROR("create epollfd failed %s", strerror(errno));
        }
        return fd;
    }

    EPollPoller::EPollPoller(EventLoop *loop)
        : Poller(loop),
          _epollfd(creatfd()),
          _events(KInitEventListSize) {}

    EPollPoller ::~EPollPoller() { ::close(_epollfd); }

    Timestamp EPollPoller::poll(int timeoutMS, ChannelList *activechannels)
    {
        Timestamp now = Timestamp::now();
        int n = epoll_wait(_epollfd, &_events[0], _events.size(), timeoutMS);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                LOG_DEBUG("epoll signal was interrupt");
                return now;
            }
            LOG_ERROR("epoll wait erro : %s ", strerror(errno));
            return now;
        }
        else if (n == 0)
        {
            LOG_DEBUG("epoll timeout");
            return now;
        }
        fillActiveChannels(n, activechannels);

        if (n == _events.size())
        {
            _events.resize(_events.size() * 2);
        }
        return now;
    }

    void EPollPoller::updateChannel(Channel *channel)
    {
        if (channel->index() == KNew || channel->index() == KDeleted)
        {
            int fd = channel->fd();
            if (channel->index() == KNew)
            {
                assert(_channels.find(fd) == _channels.end());
                _channels[fd] = channel;
            }
            else
            {
                assert(_channels.find(fd) != _channels.end());
                assert(_channels[fd] == channel);
            }
            update(EPOLL_CTL_ADD, channel);
            channel->setindex(KAdded);
        }
        else
        {
            if (channel->events() == KNonEvent)
            {
                update(EPOLL_CTL_DEL, channel);
                channel->setindex(KDeleted);
            }
            else
            {
                update(EPOLL_CTL_MOD, channel);
            }
        }
    }
    void EPollPoller::removeChannel(Channel *channel)
    {
        int fd = channel->fd();
        assert(_channels.find(fd) != _channels.end());
        assert(_channels[fd] == channel);

        if (channel->index() == KAdded)
        {
            update(EPOLL_CTL_DEL, channel);
        }
        _channels.erase(fd);
        channel->setindex(KNew);
    }

    void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activechannels)
    {
        for (int i = 0; i < numEvents; i++)
        {
            Channel *channel = (Channel *)_events[i].data.ptr;
            channel->setRevent(_events[i].events);
            activechannels->push_back(channel);
        }
    }
    void EPollPoller::update(int operation, Channel *channel)
    {
        int fd = channel->fd();
        int events = channel->events();

        struct epoll_event ev;
        ev.data.ptr = channel;
        ev.events = events;

        int ret = epoll_ctl(_epollfd, operation, fd, &ev);
        if (ret < 0)
        {
            LOG_ERROR("epoll_ctl err %s", strerror(errno));
        }
    }
} // namespace net
