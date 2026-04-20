// VinylView.cpp
#include "VinylView.h"

VinylView::VinylView()
{
    startTimerHz(30);
}

VinylView::~VinylView()
{
    stopTimer();
}

void VinylView::setFile(const juce::File& file)
{
    albumArt = tryLoadAlbumArt(file);
    angle = 0.0f;
    repaint();
}

void VinylView::clear()
{
    albumArt = juce::Image();
    angle = 0.0f;
    repaint();
}

void VinylView::setPlaying(bool shouldPlay)
{
    playing = shouldPlay;
}

void VinylView::timerCallback()
{
    if (playing)
    {
        // 33.33 RPM => ~0.1164 rad per 30fps tick
        angle += 0.1164f;
        if (angle > juce::MathConstants<float>::twoPi)
            angle -= juce::MathConstants<float>::twoPi;
        repaint();
    }
}

void VinylView::paint(juce::Graphics& g)
{
    auto bounds  = getLocalBounds().toFloat();
    float sz     = juce::jmin(bounds.getWidth(), bounds.getHeight()) - 4.0f;
    auto  centre = bounds.getCentre();
    auto  vinyl  = juce::Rectangle<float>(sz, sz).withCentre(centre);

    {
        juce::Graphics::ScopedSaveState saved(g);
        g.addTransform(juce::AffineTransform::rotation(angle, centre.x, centre.y));

        // Body
        g.setColour(juce::Colour(0xFF101010));
        g.fillEllipse(vinyl);

        // Grooves (concentric rings)
        for (float r = sz * 0.135f; r < sz * 0.486f; r += sz * 0.022f)
        {
            g.setColour(juce::Colour(0xFF1E1E1E));
            g.drawEllipse(vinyl.reduced(r), 0.7f);
            g.setColour(juce::Colour(0xFF141414));
            g.drawEllipse(vinyl.reduced(r + sz * 0.008f), 0.4f);
        }

        // Subtle sheen
        g.setColour(juce::Colour(0xFF6EE7FF).withAlpha(0.03f));
        g.fillEllipse(vinyl);

        // Label area (centre circle) — album art or neon default
        float labelR = sz * 0.30f;
        auto  labelRect = juce::Rectangle<float>(labelR * 2.0f, labelR * 2.0f).withCentre(centre);

        {
            juce::Path clip;
            clip.addEllipse(labelRect);
            juce::Graphics::ScopedSaveState ss2(g);
            g.reduceClipRegion(clip);

            if (albumArt.isValid())
            {
                // Solid background first so no dark vinyl bleeds through art edges
                g.setColour(juce::Colours::black);
                g.fillEllipse(labelRect);

                g.drawImageWithin(albumArt,
                    (int)labelRect.getX(), (int)labelRect.getY(),
                    (int)labelRect.getWidth(), (int)labelRect.getHeight(),
                    juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
            }
            else
            {
                g.setColour(juce::Colour(0xFF120A40));
                g.fillEllipse(labelRect);
                g.setColour(juce::Colour(0xFF6EE7FF).withAlpha(0.55f));
                g.drawEllipse(labelRect.reduced(2.0f), 1.5f);
                g.setColour(juce::Colour(0xFF6EE7FF).withAlpha(0.20f));
                g.drawEllipse(labelRect.reduced(labelR * 0.35f), 1.0f);
            }
        }

        // Spindle hole
        float holeR = sz * 0.024f;
        auto  hole  = juce::Rectangle<float>(holeR * 2.0f, holeR * 2.0f).withCentre(centre);
        g.setColour(juce::Colour(0xFF060606));
        g.fillEllipse(hole);
        g.setColour(juce::Colour(0xFF3A3A3A));
        g.drawEllipse(hole, 0.5f);

        // Outer rim
        g.setColour(juce::Colour(0xFF3A3A3A));
        g.drawEllipse(vinyl.reduced(0.8f), 1.2f);
    }
}

// Static: parse ID3v2 APIC frame to extract album art from MP3 files
juce::Image VinylView::tryLoadAlbumArt(const juce::File& file)
{
    auto ext = file.getFileExtension().toLowerCase();
    if (ext != ".mp3") return {};

    juce::FileInputStream stream(file);
    if (!stream.openedOk()) return {};

    // ID3v2 header is 10 bytes
    uint8_t hdr[10] = {};
    if (stream.read(hdr, 10) < 10) return {};
    if (hdr[0] != 'I' || hdr[1] != 'D' || hdr[2] != '3') return {};

    const int  majorVer      = hdr[3];
    const bool hasExtHeader  = (hdr[5] & 0x40) != 0;

    int64_t tagSize = ((int64_t)(hdr[6] & 0x7f) << 21)
                    | ((int64_t)(hdr[7] & 0x7f) << 14)
                    | ((int64_t)(hdr[8] & 0x7f) <<  7)
                    | ((int64_t)(hdr[9] & 0x7f));

    if (hasExtHeader)
    {
        uint8_t eb[4] = {};
        if (stream.read(eb, 4) < 4) return {};
        int64_t extSz;
        if (majorVer >= 4)
            extSz = ((int64_t)(eb[0]&0x7f)<<21)|((int64_t)(eb[1]&0x7f)<<14)
                  | ((int64_t)(eb[2]&0x7f)<< 7)|((int64_t)(eb[3]&0x7f));
        else
            extSz = ((int64_t)eb[0]<<24)|((int64_t)eb[1]<<16)
                  | ((int64_t)eb[2]<< 8)|((int64_t)eb[3]);
        stream.skipNextBytes(extSz - 4);
        tagSize -= extSz;
    }

    const int64_t tagEnd = 10 + tagSize;

    while (stream.getPosition() < tagEnd - 10)
    {
        uint8_t fid[4] = {};
        if (stream.read(fid, 4) < 4) break;
        if (fid[0] == 0) break; // padding

        uint8_t sb[4] = {};
        if (stream.read(sb, 4) < 4) break;

        int64_t frameSize;
        if (majorVer >= 4)
            frameSize = ((int64_t)(sb[0]&0x7f)<<21)|((int64_t)(sb[1]&0x7f)<<14)
                      | ((int64_t)(sb[2]&0x7f)<< 7)|((int64_t)(sb[3]&0x7f));
        else
            frameSize = ((int64_t)sb[0]<<24)|((int64_t)sb[1]<<16)
                      | ((int64_t)sb[2]<< 8)|((int64_t)sb[3]);

        stream.skipNextBytes(2); // frame flags

        if (frameSize <= 0 || frameSize > 20 * 1024 * 1024)
        {
            if (frameSize > 0) stream.skipNextBytes(frameSize);
            continue;
        }

        const bool isAPIC = (fid[0]=='A' && fid[1]=='P' && fid[2]=='I' && fid[3]=='C');
        if (!isAPIC) { stream.skipNextBytes(frameSize); continue; }

        juce::MemoryBlock mb((size_t)frameSize);
        if (stream.read(mb.getData(), (int)frameSize) < frameSize) break;

        const auto* d = (const uint8_t*)mb.getData();
        int pos = 0;
        const int enc = d[pos++]; // text encoding

        // skip MIME type (always Latin-1 null-terminated)
        while (pos < (int)frameSize && d[pos] != 0) ++pos;
        if (pos < (int)frameSize) ++pos; // skip null

        if (pos < (int)frameSize) ++pos; // skip picture type

        // skip description (encoding-aware null terminator)
        if (enc == 0 || enc == 3)
        {
            while (pos < (int)frameSize && d[pos] != 0) ++pos;
            if (pos < (int)frameSize) ++pos;
        }
        else // UTF-16: double-null
        {
            while (pos + 1 < (int)frameSize && (d[pos] != 0 || d[pos+1] != 0)) pos += 2;
            if (pos + 1 < (int)frameSize) pos += 2;
        }

        if (pos < (int)frameSize)
        {
            juce::MemoryInputStream imgStream(d + pos, (size_t)(frameSize - pos), false);
            auto img = juce::ImageFileFormat::loadFrom(imgStream);
            if (img.isValid()) return img;
        }
        break;
    }

    return {};
}
