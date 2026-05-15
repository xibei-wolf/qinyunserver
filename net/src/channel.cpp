#include "channel.h"
#include "eventloop.h"
#include "enum.h"
#include "timestamp.h"
#include <assert.h>
namespace net
{
    Channel::Channel(EventLoop *loop, int fd)
        : _loop(loop),
          _fd(fd),
          _events(KNonEvent),
          _revents(KNonEvent),
          _index(KNew),
          _tied(false),
          _eventHandling(false),
          _addedToLoop(false)
    {
        LOG_DEBUG("new channel %lu ", (uint64_t)this);
    }
    Channel ::~Channel()
    {
        assert(_eventHandling == false);
        assert(_addedToLoop == false);
        LOG_DEBUG("delet channel %lu", (uint64_t)this);
    }

    void Channel::update()
    {
        _addedToLoop = true;
        _loop->updateChannel(this);
    }
    
    void Channel::remove()
    {
        _loop->removeChannel(this);
        _addedToLoop = false;
    }
}