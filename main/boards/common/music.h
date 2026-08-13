#ifndef MUSIC_H
#define MUSIC_H

#include <string>
#include <cstdint>

class Music {
public:
    virtual ~Music() = default;

    virtual bool Download(const std::string& song_name, const std::string& artist_name = "") = 0;
    virtual std::string GetDownloadResult() = 0;
    virtual bool StartStreaming(const std::string& music_url) = 0;
    virtual bool StopStreaming() = 0;
    virtual bool IsPlaying() const = 0;

    virtual bool Pause() = 0;
    virtual bool Resume() = 0;
    virtual bool Next() = 0;
    virtual bool Prev() = 0;
    virtual int32_t CurrentMusicId() const = 0;
};

#endif  // MUSIC_H
