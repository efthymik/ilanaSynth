#pragma once

// M8.7: a Physical oscillator's animated view, in its OSC card's well:
// the string (its modes, shaped by the strike point, the felt or pick and
// the stiffness, decaying with DECAY and DAMP), the hammer, pick or bow
// that excites it, and the body underneath, glowing with the output. It is
// a picture of the model's settings, restarted by every note; slowed down
// so the motion can be seen.

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <cmath>
#include <utility>
#include <vector>

#include "../PluginProcessor.h"
#include "IlanaLookAndFeel.h"
#include "AnimationUtils.h"

// The exciters (oscN_excite) by one set of names on every page, and in
// groups for the menus (UI review 7, I7-27). Choice 5, the M4 hammer kept
// for old patches, reads "Bright Hammer" beside the Piano Hammer (display
// only: the parameter's choices are unchanged).
namespace Exciters
{
inline const juce::StringArray& names()
{
    static const juce::StringArray list { "Burst", "Noise", "Saw", "Pulse", "Bow", "Bright Hammer", "Osc In",
                                          "Tine", "Reed", "Piano Hammer", "Feedback", "Modal Piano",
                                          "Modal Guitar", "Modal Bowed" };
    return list;
}

inline juce::String name (int excite) { return names()[juce::jlimit (0, names().size() - 1, excite)]; }

inline const std::vector<std::pair<const char*, std::vector<int>>>& groups()
{
    static const std::vector<std::pair<const char*, std::vector<int>>> list {
        { "PLUCK", { 0, 1, 2, 3, 12 } }, { "STRIKE", { 9, 11, 5 } }, { "BOW", { 4, 13 } }, { "ELECTRIC PIANO", { 7, 8 } }, { "DRIVEN", { 10, 6 } }
    };
    return list;
}
} // namespace Exciters

class PhysicalView : public juce::Component,
                     public juce::SettableTooltipClient,
                     private IlanaAnim::FrameTimer
{
public:
    explicit PhysicalView (IlanaSynthAudioProcessor& p, juce::String oscPrefix = "osc1")
        : processorRef (p), prefix (std::move (oscPrefix))
    {
        setTooltip ("The string, its exciter and the body, drawn from the oscillator's settings and restarted by every "
                    "note (slowed down so the motion shows). STIFF bends the partials, EXCITE POS moves the strike, "
                    "DECAY and DAMP set how the modes die away.");
        startTimerHz (40);
    }

    void setOscillator (const juce::String& newPrefix) { prefix = newPrefix; restart(); }
    // The picture alone, small (an OSC card's well): no captions, a slim body.
    void setCompact (bool shouldBeCompact) { compact = shouldBeCompact; repaint(); }
    const juce::String& getOscillator() const { return prefix; } // (the UI test reads it)
    // Drawn in the oscillator's identity colour, like the rest of its page.
    void setColour (juce::Colour newColour) { colour = newColour; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        IlanaTheme::paintWell (g, bounds, 8.0f);
        const auto accent = colour;
        const auto excite = (int) read ("_excite");
        // The modal guitar and the bowed string carry their own body: the instrument is drawn at the strip's
        // left, as big as SIZE and SCALE make it, and named (UI review 30).
        const auto modal = excite == 12 || excite == 13;
        auto area = compact ? bounds.reduced (8.0f, modal ? 3.0f : 8.0f) : bounds.reduced (18.0f, 14.0f);
        // The body a slim band under the string, so the string keeps the
        // room (UI review 7, S7-39).
        // (The compact picture, PLAY's, has no band at all while there is no body: the string takes it.)
        const auto noBandNeeded = compact && (modal || ! hasBody());
        const auto bodyArea = area.removeFromBottom (noBandNeeded ? 0.0f : compact ? juce::jlimit (10.0f, 18.0f, area.getHeight() * 0.2f)
                                                                                    : juce::jlimit (26.0f, 48.0f, area.getHeight() * 0.16f));
        area.removeFromBottom (noBandNeeded ? 0.0f : compact ? 4.0f : 8.0f);
        // The page's picture keeps a footer for the string's ruler (A16-4).
        const auto footer = compact ? juce::Rectangle<float>() : area.removeFromBottom (13.0f);
        const auto stringY = area.getCentreY() + (modal && compact ? 0.0f : area.getHeight() * 0.12f);
        auto left = area.getX() + 12.0f;
        const auto right = area.getRight() - 12.0f;
        if (modal)
            left = juce::jmin (right - 120.0f, paintInstrument (g, area, excite == 13, accent) + 22.0f);
        const auto strike = excitePosition();

        // The page's picture also carries the string's anatomy (A16-4): its
        // first three standing modes ghosted behind it, a ruler along the
        // foot with a tick every sixteenth of the length, and the strike
        // point marked on the string and the ruler.
        if (! compact)
        {
            for (int mode = 1; mode <= 3; ++mode)
            {
                juce::Path ghost;
                for (int i = 0; i <= 120; ++i)
                {
                    const auto t = (float) i / 120.0f;
                    const auto point = juce::Point<float> (left + (right - left) * t,
                                                           stringY - std::sin (juce::MathConstants<float>::pi * (float) mode * t) * area.getHeight() * (0.26f - 0.05f * (float) mode));
                    if (i == 0) ghost.startNewSubPath (point); else ghost.lineTo (point);
                }
                g.setColour (accent.withAlpha (0.16f));
                g.strokePath (ghost, juce::PathStrokeType (1.0f));
            }

            const auto rulerY = footer.getY() + 2.0f;
            g.setColour (IlanaTheme::Ui::line);
            g.drawHorizontalLine ((int) rulerY, left, right);
            for (int tick = 0; tick <= 16; ++tick)
                g.fillRect (left + (right - left) * (float) tick / 16.0f - 0.5f, rulerY, 1.0f, tick % 4 == 0 ? 4.0f : 2.0f);

            const auto strikeX = left + (right - left) * strike;
            juce::Path guide, guideLine;
            guideLine.startNewSubPath (strikeX, stringY);
            guideLine.lineTo (strikeX, rulerY);
            const float guideDashes[] { 3.0f, 3.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (guide, guideLine, guideDashes, 2);
            g.setColour (accent.withAlpha (0.55f));
            g.fillPath (guide);
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ strikeX, stringY }));
            g.setFont (IlanaTheme::font (IlanaTheme::TextSize::tiny, true));
            g.setColour (IlanaTheme::Ui::text2);
            g.drawText ("STRIKE " + juce::String (juce::roundToInt (strike * 100.0f)) + "%",
                        juce::Rectangle<float> (90.0f, 12.0f).withPosition (strikeX + 6.0f, rulerY + 1.0f).toNearestInt(), juce::Justification::centredLeft);
            g.setColour (IlanaTheme::Ui::text3);
            g.drawText ("NUT", juce::Rectangle<float> (40.0f, 12.0f).withPosition (left, rulerY + 1.0f).toNearestInt(), juce::Justification::centredLeft);
            g.drawText ("BRIDGE", juce::Rectangle<float> (50.0f, 12.0f).withPosition (right - 50.0f, rulerY + 1.0f).toNearestInt(), juce::Justification::centredRight);
        }

        // Nut and bridge.
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.fillRoundedRectangle (left - 6.0f, stringY - 16.0f, 6.0f, 32.0f, 2.0f);
        g.fillRoundedRectangle (right, stringY - 12.0f, 8.0f, 24.0f, 2.0f);

        // The string: the sum of its modes.
        juce::Path string;
        constexpr int points = 160;
        const auto amplitude = area.getHeight() * 0.3f;
        for (int i = 0; i <= points; ++i)
        {
            const auto x = (double) i / points;
            auto y = 0.0;
            for (int n = 1; n <= numModes; ++n)
                y += modeLevel[(size_t) n - 1] * std::sin (juce::MathConstants<double>::pi * n * x) * std::cos (modePhase[(size_t) n - 1]);
            const auto px = left + (right - left) * (float) x;
            const auto py = stringY - (float) y * amplitude;
            if (i == 0) string.startNewSubPath (px, py); else string.lineTo (px, py);
        }
        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (string, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved));
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.strokePath (string, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));

        // The exciter at its position.
        const auto ex = left + (right - left) * strike;
        if (excite == 4 || excite == 13) // bow
        {
            const auto sway = (float) std::sin (clock * 3.0) * 10.0f;
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.drawLine (ex - 40.0f + sway, stringY - 6.0f, ex + 40.0f + sway, stringY + 6.0f, 3.0f);
        }
        else if (excite == 5 || excite == 9 || excite == 11) // hammers
        {
            const auto lift = hammerLift();
            // (Held inside the picture: on PLAY's short STRING strip the raised head went past the top, UI review 28.)
            const auto headH = juce::jmin (14.0f, area.getHeight() * 0.4f);
            const auto head = juce::Rectangle<float> (headH * 18.0f / 14.0f, headH)
                                  .withCentre ({ ex, juce::jmax (area.getY() + headH * 0.5f + 1.0f, stringY - 12.0f - lift * area.getHeight() * 0.35f) });
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.drawLine (head.getCentreX(), head.getY(), head.getCentreX() - 30.0f, area.getY(), 2.0f);
            g.setColour (accent);
            g.fillRoundedRectangle (head, 5.0f);
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.fillRoundedRectangle (head.withTrimmedTop (head.getHeight() * 0.55f), 4.0f);
        }
        else if (excite == 10) // feedback: the amp, pushing sound back at the string
        {
            const auto amp = juce::Rectangle<float> (46.0f, 40.0f).withCentre ({ right - 40.0f, area.getY() + 30.0f });
            g.setColour (juce::Colours::white.withAlpha (0.3f));
            g.fillRoundedRectangle (amp, 4.0f);
            g.setColour (accent.withAlpha (0.8f));
            g.drawEllipse (amp.reduced (9.0f, 6.0f), 2.0f);
            const auto glow = juce::jlimit (0.0f, 1.0f, bodyGlow);
            for (int wave = 0; wave < 3; ++wave)
            {
                const auto r = 14.0f + (float) std::fmod (clock * 30.0 + wave * 14.0, 42.0);
                juce::Path arc;
                arc.addCentredArc (amp.getX(), amp.getCentreY(), r, r, 0.0f, -juce::MathConstants<float>::pi * 0.85f,
                                   -juce::MathConstants<float>::pi * 0.15f, true);
                g.setColour (accent.withAlpha ((0.15f + 0.6f * glow) * (1.0f - (r - 14.0f) / 42.0f)));
                g.strokePath (arc, juce::PathStrokeType (1.5f));
            }
            juce::Path pick;
            const auto tip = juce::Point<float> (ex, stringY - 4.0f - hammerLift() * 20.0f);
            pick.addTriangle (tip, tip.translated (-9.0f, -18.0f), tip.translated (9.0f, -18.0f));
            g.setColour (accent);
            g.fillPath (pick);
        }
        else if (excite == 7 || excite == 8) // tine or reed
        {
            g.setColour (accent);
            g.fillRoundedRectangle (juce::Rectangle<float> (8.0f, 22.0f).withCentre ({ ex, stringY - 22.0f + hammerLift() * 12.0f }), 3.0f);
        }
        else // pluck
        {
            juce::Path pick;
            // (Held inside the picture: on PLAY's short STRING strip the raised pick went past the top, as the
            // hammer did in UI review 28, so a plucked string showed no exciter at all. UI review 30.)
            const auto pickH = juce::jmin (18.0f, area.getHeight() * 0.42f);
            const auto tip = juce::Point<float> (ex, juce::jmax (area.getY() + pickH + 1.0f, stringY - 4.0f - hammerLift() * juce::jmin (20.0f, area.getHeight() * 0.3f)));
            pick.addTriangle (tip, tip.translated (-pickH * 0.5f, -pickH), tip.translated (pickH * 0.5f, -pickH));
            g.setColour (accent);
            g.fillPath (pick);
        }

        // The body: a plate with its nodal lines, glowing with the output.
        // With no body there is nothing to draw: a faint dashed outline
        // marks where one would sit.
        g.setFont (IlanaTheme::font (IlanaTheme::TextSize::label, true));

        if (hasBody())
        {
            const auto glow = juce::jlimit (0.0f, 1.0f, bodyGlow);
            IlanaTheme::paintGlow (g, bodyArea, 10.0f, accent, glow * 1.5f);
            g.setColour (IlanaTheme::Ui::well.interpolatedWith (accent, 0.06f + 0.2f * glow));
            g.fillRoundedRectangle (bodyArea, 10.0f);
            g.setColour (IlanaTheme::Ui::line.interpolatedWith (accent, 0.6f * glow));
            g.drawRoundedRectangle (bodyArea, 10.0f, 1.2f);
            for (int line = 1; line < 5; ++line)
            {
                const auto t = (float) line / 5.0f;
                const auto wobble = (float) std::sin (clock * 5.0 + line) * 3.0f * glow;
                g.drawLine (bodyArea.getX() + bodyArea.getWidth() * t + wobble, bodyArea.getY() + 6.0f,
                            bodyArea.getX() + bodyArea.getWidth() * t - wobble, bodyArea.getBottom() - 6.0f, 0.8f);
            }
            g.setColour (IlanaTheme::Ui::text2);
        }
        else if (! noBandNeeded)
        {
            juce::Path outline;
            outline.addRoundedRectangle (bodyArea, 10.0f);
            juce::Path dashed;
            const float dashes[] { 4.0f, 5.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
            g.setColour (IlanaTheme::Ui::line.withAlpha (0.6f));
            g.fillPath (dashed);
            g.setColour (IlanaTheme::Ui::text3);
        }

        if (! compact)
        {
            g.drawText (bodyName(), bodyArea.reduced (12.0f, 2.0f), juce::Justification::centredLeft);
            g.setColour (IlanaTheme::Ui::text2);
            g.drawText (exciteName (excite), area.withHeight (16.0f), juce::Justification::topRight);
        }

        // Not a Physical oscillator: the picture is only what it would be.
        if (juce::roundToInt (read ("_mode")) != 1)
        {
            g.setColour (IlanaTheme::Ui::well.withAlpha (0.5f));
            g.fillRoundedRectangle (bounds, 8.0f);
            // The card beside it says why and offers the switch; the picture
            // carries no label (review 10, I10-12).
        }
    }

private:
    bool compact = false;
    static constexpr int numModes = 16;

    float read (const char* suffix) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (prefix + suffix))
            return value->load();
        return 0.0f;
    }

    float readGlobal (const char* id) const
    {
        if (const auto* value = processorRef.apvts.getRawParameterValue (id))
            return value->load();
        return 0.0f;
    }

    // The modal instruments' own picture: the body as big as SIZE makes it, the neck as long as SCALE, and
    // beside it the name of the size (the engine's marks: violin x1, viola x1.17, cello x2.1) with both
    // factors. Fills a fixed zone at the left of `area` whatever the size, so the string beside it never
    // jumps; returns the zone's right edge.
    float paintInstrument (juce::Graphics& g, juce::Rectangle<float> area, bool bowed, juce::Colour accent) const
    {
        const auto size = juce::jlimit (0.25f, 4.0f, read ("_modal_size"));
        const auto scale = juce::jlimit (0.5f, 2.0f, read ("_modal_scale"));
        const auto sizeT = (std::log2 (size) + 2.0f) / 4.0f;   // 0 at x0.25, 1 at x4
        const auto scaleT = std::log2 (scale) + 1.0f;          // 0 at x0.5, 1 at x1, 2 at x2
        const auto maxH = juce::jmin (area.getHeight(), 60.0f);
        const auto aspect = bowed ? 1.7f : 1.45f;
        const auto bodyH = maxH * (0.28f + 0.72f * std::pow (sizeT, 1.5f)); // (steep enough that a cello reads as half again a violin)
        const auto bodyL = bodyH * aspect;
        const auto maxBodyL = maxH * aspect;
        const auto neckL = maxH * (0.9f + 0.75f * scaleT);
        const auto maxNeckL = maxH * 2.4f;
        const auto headL = maxH * 0.42f;
        const auto midY = area.getCentreY();
        const auto bodyX = area.getX() + 4.0f + (maxBodyL - bodyL) * 0.5f;

        // The body's outline: two bouts and a waist, the lower bout (at the tail, left) the wider.
        const auto halfHeight = [&] (float u)
        {
            const auto round = std::pow (std::sin (juce::MathConstants<float>::pi * juce::jlimit (0.0f, 1.0f, u)), 0.5f);
            const auto bout = 1.0f - 0.2f * u;
            const auto waist = 1.0f - (bowed ? 0.36f : 0.22f) * std::exp (-std::pow ((u - 0.54f) / 0.13f, 2.0f));
            return bodyH * 0.5f * round * bout * waist;
        };
        juce::Path body;
        constexpr int steps = 48;
        for (int i = 0; i <= steps; ++i)
        {
            const auto u = (float) i / (float) steps;
            const juce::Point<float> point (bodyX + bodyL * u, midY - halfHeight (u));
            if (i == 0) body.startNewSubPath (point); else body.lineTo (point);
        }
        for (int i = steps; i >= 0; --i)
        {
            const auto u = (float) i / (float) steps;
            body.lineTo (bodyX + bodyL * u, midY + halfHeight (u));
        }
        body.closeSubPath();

        const auto glow = juce::jlimit (0.0f, 1.0f, bodyGlow);
        // The neck and the head behind the body.
        const auto neckX = bodyX + bodyL * 0.94f;
        const auto neckH = juce::jmax (2.0f, bodyH * 0.13f);
        g.setColour (accent.withAlpha (0.55f));
        g.fillRect (juce::Rectangle<float> (neckX, midY - neckH * 0.5f, neckL, neckH));
        if (bowed) // the scroll
            g.fillEllipse (juce::Rectangle<float> (headL * 0.55f, headL * 0.55f).withCentre ({ neckX + neckL + headL * 0.2f, midY }));
        else       // the head, with its pegs' side wider
            g.fillRoundedRectangle (juce::Rectangle<float> (neckX + neckL - 1.0f, midY - neckH * 1.1f, headL, neckH * 2.2f), 1.5f);

        g.setColour (IlanaTheme::Ui::well.interpolatedWith (accent, 0.14f + 0.22f * glow));
        g.fillPath (body);
        g.setColour (accent.withAlpha (0.9f));
        g.strokePath (body, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved));

        // What tells them apart at a glance: the guitar's round hole, the bowed body's two f-holes.
        const auto bridgeX = bodyX + bodyL * 0.3f;
        g.setColour (accent.withAlpha (0.75f));
        if (bowed)
        {
            for (const auto side : { -1.0f, 1.0f })
                g.drawLine (bodyX + bodyL * 0.38f, midY + side * bodyH * 0.2f, bodyX + bodyL * 0.6f, midY + side * bodyH * 0.14f, juce::jmax (1.0f, bodyH * 0.05f));
        }
        else
            g.drawEllipse (juce::Rectangle<float> (bodyH * 0.3f, bodyH * 0.3f).withCentre ({ bodyX + bodyL * 0.64f, midY }), 1.2f);
        g.fillRect (juce::Rectangle<float> (juce::jmax (1.5f, bodyL * 0.035f), bodyH * (bowed ? 0.22f : 0.34f)).withCentre ({ bridgeX, midY }));
        // Its string, bridge to nut.
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.drawLine (bridgeX, midY, neckX + neckL, midY, 1.0f);

        // The name of the size and the two factors, on a fixed column.
        const auto textX = area.getX() + 4.0f + maxBodyL + maxNeckL - maxBodyL * 0.06f + headL + 10.0f;
        const auto nameFont = IlanaTheme::font (IlanaTheme::TextSize::label, true);
        const auto textWidth = 150.0f;
        const auto lineH = juce::jmin (area.getHeight() * 0.5f, nameFont.getHeight() + 2.0f);
        auto text = juce::Rectangle<float> (textX, midY - lineH, textWidth, lineH * 2.0f);
        g.setFont (nameFont);
        g.setColour (IlanaTheme::Ui::text);
        g.drawText (instrumentName (bowed, size), text.removeFromTop (lineH).toNearestInt(), juce::Justification::centredLeft);
        g.setColour (IlanaTheme::Ui::text2);
        g.drawText (juce::String::fromUTF8 ("BODY \xc3\x97") + factorText (size) + juce::String::fromUTF8 ("  NECK \xc3\x97") + factorText (scale),
                    text.toNearestInt(), juce::Justification::centredLeft);
        return textX + textWidth;
    }

    static juce::String factorText (float factor)
    {
        return juce::String (factor, std::abs (factor * 10.0f - std::round (factor * 10.0f)) < 0.05f ? 1 : 2);
    }

public:
    // The engine's instrument marks on SIZE (ModalBowedBodies.h): the violin is x1, the viola x1.17, the cello x2.1.
    static constexpr float violaSize = 1.17f, celloSize = 2.1f;

    // What a modal body of this SIZE is called: an instrument at its mark, else where it sits between them.
    static juce::String instrumentName (bool bowed, float size)
    {
        const auto atMark = [size] (float mark) { return std::abs (std::log2 (size / mark)) < 0.045f; };
        const auto to = juce::String::fromUTF8 (" \xe2\x80\xba ");
        if (! bowed)
            return atMark (1.0f) ? "NYLON GUITAR" : size < 1.0f ? "SMALL GUITAR" : "BIG GUITAR";
        if (atMark (1.0f)) return "VIOLIN";
        if (atMark (violaSize)) return "VIOLA";
        if (atMark (celloSize)) return "CELLO";
        return size < 1.0f ? juce::String ("SMALL VIOLIN") : size < violaSize ? "VIOLIN" + to + "VIOLA" : size < celloSize ? "VIOLA" + to + "CELLO" : juce::String ("BIG CELLO");
    }

private:
    float excitePosition() const
    {
        const auto position = read ("_string_excite_pos");
        return position > 0.0f ? juce::jlimit (0.02f, 0.5f, position) : 0.125f;
    }

    float hammerLift() const
    {
        // Down onto the string at the note, thrown back up after it.
        const auto t = (float) sinceNote;
        return t < 0.05f ? 1.0f - t / 0.05f : juce::jmin (1.0f, (t - 0.05f) / 0.25f);
    }

    bool hasBody() const { return readGlobal ("sb_on") > 0.5f || readGlobal ("res_on") > 0.5f; }

    juce::String bodyName() const
    {
        const juce::StringArray bodies { "CLASSIC BODY", "BAR", "PLATE", "BELL", "SHELL", "GUITAR BODY" };
        const auto type = (int) readGlobal ("body_type");
        if (readGlobal ("sb_on") > 0.5f)
            return readGlobal ("sb_model") > 0.5f ? "SOUNDBOARD (DENSE)" : "SOUNDBOARD";
        return readGlobal ("res_on") > 0.5f ? bodies[juce::jlimit (0, bodies.size() - 1, type)] : "NO BODY";
    }

    static juce::String exciteName (int excite) { return Exciters::name (excite).toUpperCase(); }

    void restart()
    {
        // Mode levels from the strike point and the exciter's hardness.
        const auto strike = (double) excitePosition();
        const auto hardness = (double) read ("_hammer_hard");
        const auto rolloff = 1.2 + 1.2 * (1.0 - hardness);
        for (int n = 1; n <= numModes; ++n)
        {
            modeLevel[(size_t) n - 1] = std::sin (juce::MathConstants<double>::pi * n * strike) / std::pow ((double) n, rolloff) * 1.4;
            modePhase[(size_t) n - 1] = 0.0;
        }
        sinceNote = 0.0;
    }

    void timerCallback() override
    {
        const auto dt = (double) frameSeconds();
        clock += dt;
        sinceNote += dt;

        const auto notes = processorRef.getNoteOnCount();
        if (notes != lastNotes)
        {
            lastNotes = notes;
            restart();
        }

        // Slowed down: the fundamental turns about once a second; stiffness
        // speeds the upper modes up (n sqrt (1 + B n^2)); DECAY and DAMP
        // take them away (the upper ones faster).
        const auto stiffness = (double) read ("_string_stiffness");
        const auto b = 1.0e-4 * std::pow (10.0, 2.5 * stiffness);
        const auto decay = (double) read ("_string_decay");
        const auto damp = (double) read ("_string_damp");
        // The last ten pixels or so of a long ring-out are barely visible but keep
        // the page repainting for tens of seconds, so they fade a little
        // faster (no step: the extra damping is constant below 12 px).
        auto shownTotal = 0.0;
        for (const auto level : modeLevel)
            shownTotal += std::abs (level);
        const auto tailRate = shownTotal * (double) getHeight() * 0.3 < 12.0 ? 0.8 : 0.0;
        for (int n = 1; n <= numModes; ++n)
        {
            modePhase[(size_t) n - 1] += juce::MathConstants<double>::twoPi * 0.9 * n * std::sqrt (1.0 + b * n * n) * dt;
            const auto seconds = (0.4 + 6.0 * decay) / (1.0 + damp * 0.4 * (n - 1));
            modeLevel[(size_t) n - 1] *= std::exp (-dt * (1.0 / seconds + tailRate));
        }
        // A held note that keeps sounding (bow, feedback, SUSTAIN) keeps the
        // string moving with the output.
        const auto level = juce::jmin (1.0f, processorRef.getOutputPeak() * 2.0f);
        if (sinceNote > 0.3 && level > 0.05f)
            for (int n = 1; n <= numModes; ++n)
                modeLevel[(size_t) n - 1] = juce::jmax (modeLevel[(size_t) n - 1],
                                                        0.5 * level * std::abs (std::sin (juce::MathConstants<double>::pi * n * excitePosition())) / (n * n));
        bodyGlow = IlanaAnim::approach (bodyGlow, juce::jmin (1.0f, processorRef.getOutputPeak() * 2.0f), 0.15f, frameTicks());

        // Another mode's preview plucks itself now and then (while shown),
        // so the page shows what the switch would give.
        // It does so once after the page opens or a setting changes, then
        // rests (a preview that never stopped kept the page repainting).
        const auto shown = IlanaAnim::showing (*this);
        const auto epoch = processorRef.getUiEpoch();
        if (! shown || epoch != lastEpoch)
        {
            lastEpoch = epoch;
            autoPlucks = 0;
        }

        if (shown && autoPlucks < 1 && juce::roundToInt (read ("_mode")) != 1 && sinceNote > 3.0)
        {
            ++autoPlucks;
            restart();
        }

        // The string rests once its motion is under a third of a pixel.
        auto total = 0.0;
        for (const auto level : modeLevel)
            total += std::abs (level);
        const auto moving = bodyGlow > 0.005f || total * (double) getHeight() * 0.3 > 0.3;
        if (! moving && total > 0.0)
            modeLevel.fill (0.0);

        if (IlanaAnim::showing (*this) && (moving || changeGate.check (processorRef.getUiEpoch() ^ IlanaAnim::mouseSignature (*this))))
            repaint();
    }

    IlanaAnim::ChangeGate changeGate;
    juce::uint64 lastEpoch = 0;
    int autoPlucks = 0;

    IlanaSynthAudioProcessor& processorRef;
    juce::String prefix;
    juce::Colour colour { IlanaTheme::oscColour (0) };
    std::array<double, numModes> modeLevel {}, modePhase {};
    double clock = 0.0, sinceNote = 10.0;
    unsigned lastNotes = 0;
    float bodyGlow = 0.0f;
};

// The physical oscillator's controls, the list its OSC card lays out (UI
// review 6, S13, I6-18; the PHYSICAL tab folded into the card, 2026-10-10):
// rows named for the part of the instrument, each control as its parameter
// suffix and label, for the exciter in use (the Tine and Reed have a hammer and a pickup; the Piano
// hammer has no pick, pickup or buzz; HARDNESS and PICK POS shape only a
// plucked burst, SLAP is a pluck's or a strike's: UI review 7, I7-27).
struct PhysicalSpec
{
    const char* suffix;
    const char* label;
};

inline std::vector<std::pair<juce::String, std::vector<PhysicalSpec>>> physicalControlRows (int excite)
{
    if (excite == 7 || excite == 8)
        return { { "STRING", { { "_string_decay", "DECAY" }, { "_string_damp", "DAMP" }, { "_damper", "DAMPER" } } },
                 { "HAMMER & PICKUP", { { "_excite", "EXCITE" }, { "_hammer_hard", "HAMMER" }, { "_ep_distance", "DISTANCE" },
                                        { "_ep_position", "OFFSET" } } } };

    // The Modal Piano plays a measured grand: no string to pluck or pick up,
    // only what reshapes the recording.
    if (excite == 11)
        return { { "STRING", { { "_string_decay", "DECAY" }, { "_string_damp", "DAMP" }, { "_string_stiffness", "STIFF" },
                               { "_damper", "DAMPER" } } },
                 { "EXCITER", { { "_excite", "EXCITE" }, { "_hammer_hard", "HAMMER" }, { "_modal_size", "SIZE" },
                                { "_modal_scale", "SCALE" } } } };

    // The Modal Guitar: the same, its HAMMER the pluck.
    if (excite == 12)
        return { { "STRING", { { "_string_decay", "DECAY" }, { "_string_damp", "DAMP" }, { "_string_stiffness", "STIFF" },
                               { "_damper", "DAMPER" } } },
                 { "EXCITER", { { "_excite", "EXCITE" }, { "_hammer_hard", "PLUCK" }, { "_modal_size", "SIZE" },
                                { "_modal_scale", "SCALE" } } } };

    // The Modal Bowed string: the bow's knobs, the body's SIZE and the
    // string's SCALE; SLAP plays it pizzicato and BUZZ is its WOLF.
    if (excite == 13)
        return { { "STRING", { { "_string_decay", "DECAY" }, { "_string_damp", "DAMP" }, { "_string_stiffness", "STIFF" },
                               { "_damper", "DAMPER" } } },
                 { "EXCITER", { { "_excite", "EXCITE" }, { "_string_slap", "PIZZ" }, { "_string_excite_pos", "POSITION" },
                                { "_bow_pressure", "GRIP" }, { "_bow_speed", "SPEED" }, { "_hammer_hard", "VIBRATO" }, { "_modal_size", "SIZE" },
                                { "_modal_scale", "SCALE" }, { "_bridge_buzz", "WOLF" } } } };

    const auto piano = excite == 9;
    std::vector<PhysicalSpec> string { { "_string_decay", "DECAY" }, { "_string_damp", "DAMP" },
                                       { "_string_sustain", excite == 10 ? "FEEDBACK" : "SUSTAIN" },
                                       { "_string_stiffness", "STIFF" }, { "_register", "REGISTER" },
                                       { "_damper", "DAMPER" }, { "_couple", "COUPLE" } }; // (I8-25)
    std::vector<PhysicalSpec> exciter { { "_excite", "EXCITE" } };

    const auto plucked = excite <= 3 || excite == 10;

    if (! piano && excite != 4)
        exciter.push_back ({ "_string_slap", "SLAP" });

    exciter.push_back ({ "_string_excite_pos", "POSITION" });

    if (plucked)
        exciter.push_back ({ "_string_pick_hardness", "HARDNESS" }); // (PICK POS merged into POSITION)

    if (excite == 5 || piano)
        exciter.push_back ({ "_hammer_hard", "HAMMER" });
    else if (excite == 4)
        exciter.insert (exciter.end(), { { "_bow_pressure", "GRIP" }, { "_bow_speed", "SPEED" } });
    else if (excite == 10)
        exciter.insert (exciter.end(), { { "_fb_gain", "AMP GAIN" }, { "_fb_distance", "DISTANCE" } });

    // The bridge's buzz and the fret's rattle are the string's: they close
    // its row on the oscillator card, so the exciter's row stays one row.
    if (! piano)
    {
        exciter.push_back ({ "_string_pickup", "PICKUP" });
        if (plucked && excite != 10)
            exciter.push_back ({ "_string_click", "CLICK" }); // (the pickup hears the pluck's raw snap)
        string.insert (string.end(), { { "_bridge_buzz", "BUZZ" }, { "_fret_rattle", "RATTLE" } });
    }

    return { { "STRING", string }, { "EXCITER", exciter } };
}
