#ifndef FRAMEBUFFERCACHE_H
#define FRAMEBUFFERCACHE_H
namespace Play
{
class PlayElement;
class FrameBufferCache
{
public:
    FrameBufferCache(PlayElement* element);
    ~FrameBufferCache();

private:
    int          test = 0;
    PlayElement* _element;
};

} // namespace Play
#endif // FRAMEBUFFERCACHE_H