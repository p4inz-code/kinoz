#pragma once
#include <QFileInfo>
#include <QSet>
#include <QString>

enum class MediaKind { Unknown, Video, Audio, Image, Gif };

namespace Media {
inline const QSet<QString>& videoExt() {
    static const QSet<QString> s{"mp4", "m4v", "mkv", "avi", "webm", "mov", "wmv", "flv", "ts", "m2ts", "mts",
                                 "mpg", "mpeg", "3gp", "ogv", "vob", "divx", "f4v", "asf", "rm", "rmvb"};
    return s;
}
inline const QSet<QString>& audioExt() {
    static const QSet<QString> s{"mp3", "flac", "wav", "ogg", "oga", "opus", "m4a", "aac", "wma", "aiff", "aif",
                                 "ape", "alac", "mka", "wv", "mpc", "dsf", "ac3", "dts"};
    return s;
}
inline const QSet<QString>& imageExt() {
    static const QSet<QString> s{"jpg", "jpeg", "png", "bmp", "tif", "tiff", "webp", "avif", "heic", "heif", "jxl", "ico", "jfif"};
    return s;
}
inline const QSet<QString>& gifExt() {
    static const QSet<QString> s{"gif", "apng"};
    return s;
}
inline const QSet<QString>& subtitleExt() {
    static const QSet<QString> s{"srt", "ass", "ssa", "vtt", "sub"};
    return s;
}

inline MediaKind kindForSuffix(const QString& suffixLower) {
    if (videoExt().contains(suffixLower)) return MediaKind::Video;
    if (audioExt().contains(suffixLower)) return MediaKind::Audio;
    if (gifExt().contains(suffixLower)) return MediaKind::Gif;
    if (imageExt().contains(suffixLower)) return MediaKind::Image;
    return MediaKind::Unknown;
}
inline MediaKind kindFor(const QString& path) { return kindForSuffix(QFileInfo(path).suffix().toLower()); }
inline bool isMedia(const QString& path) { return kindFor(path) != MediaKind::Unknown; }

// Files that are browsed together: video+audio form "av", images+gifs form "pictures".
inline int family(MediaKind k) {
    switch (k) {
    case MediaKind::Video:
    case MediaKind::Audio: return 1;
    case MediaKind::Image:
    case MediaKind::Gif: return 2;
    default: return 0;
    }
}
inline bool isPicture(MediaKind k) { return k == MediaKind::Image || k == MediaKind::Gif; }
}
