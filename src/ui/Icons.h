#pragma once
#include <QColor>
#include <QPainter>
#include <QRectF>

enum class Icon {
    Play, Pause, Stop, Prev, Next, Shuffle, Repeat, RepeatOne,
    Volume0, Volume1, Volume2, Mute,
    Fullscreen, ExitFullscreen, Folder, FolderOpen, File, Video, Music, Image, Gif,
    Search, Close, Minimize, Maximize, Restore, Plus, Minus, Settings, More,
    List, Grid, Sidebar, ChevronLeft, ChevronRight, ChevronUp, ChevronDown,
    Mini, Pen, Highlighter, Arrow, Rect, Ellipse, Text, Undo, Redo, Trash, Export, Copy,
    Equalizer, Speed, Sun, Moon, Back10, Fwd10, Check, Queue, Home, Subtitles, Audio,
    Adjust, Pin, Camera, Eraser, Reveal, Drive, Up
};

// Draws an icon in a 24x24 design grid scaled into r. Pure vector: crisp at any DPI, no files, no SVG module.
void drawIcon(QPainter* p, Icon ic, const QRectF& r, const QColor& c, qreal stroke = 1.7);
