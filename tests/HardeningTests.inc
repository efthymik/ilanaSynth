// Hostile input (polish and bug pass 2026-11-16, round A): files and host
// state the synth reads must never crash, hang or eat memory. Each case is
// a file a stranger could hand over (a preset pack, a .kbm, a MIDI clip).
// Included by TableTest.cpp; ILANA_HARDENING_TEST=1 runs only these.

void runHardeningTests()
{
    const auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ilana-hardening");
    temp.deleteRecursively();
    temp.createDirectory();

    // Formula: a lone '.', an out-of-range number, deep nesting and a long
    // chain are errors, not exceptions or stack overflows.
    {
        Formula formula;
        check (formula.parse (".").isNotEmpty() && formula.parse ("x*.").isNotEmpty(), "formula: a lone '.' is an error");
        check (formula.parse ("1e999").isEmpty() && formula.evaluate ({}) == 0.0, "formula: an out-of-range number reads as 0");
        check (formula.parse (juce::String::repeatedString ("(", 5000) + "x" + juce::String::repeatedString (")", 5000)).isNotEmpty(),
               "formula: 5000 nested brackets are refused");
        check (formula.parse (juce::String::repeatedString ("-", 5000) + "x").isNotEmpty(), "formula: 5000 unary signs are refused");
        check (formula.parse ("1" + juce::String::repeatedString ("+1", 1500)).isNotEmpty(), "formula: a 1500-term chain is refused");
        check (formula.parse ("sin(2*pi*x) + 0.5").isEmpty() && std::abs (formula.evaluate ({ 0.25, 0.0, 0.0 }) - 1.5) < 1.0e-9,
               "formula: a normal formula still parses and evaluates");
        check (formula.parse ("2.5e-1*x+.5").isEmpty() && std::abs (formula.evaluate ({ 1.0, 0.0, 0.0 }) - 0.75) < 1.0e-9,
               "formula: exponents and a leading '.' still read");
        const auto frame = WavetableDoc::renderFormula (".", 0.0, 0);
        check (frame.size() == (size_t) WavetableDoc::frameSize, "formula: a broken recipe renders a (silent) frame, no crash");
    }

    // XML: deep nesting is refused before JUCE's recursive parser sees it;
    // ordinary text, comments and quoted '>' pass.
    {
        const auto deep = "<ilanaSynth>" + juce::String::repeatedString ("<a>", 50000) + juce::String::repeatedString ("</a>", 50000) + "</ilanaSynth>";
        check (! SafeXml::depthIsSafe (deep) && SafeXml::parse (deep) == nullptr, "xml: 50000 nested elements are refused");
        const auto fine = juce::String ("<?xml version=\"1.0\"?><!-- <a><a><a> --><r x=\"a>b\"><c/><d><e/></d></r>");
        check (SafeXml::depthIsSafe (fine) && SafeXml::parse (fine) != nullptr, "xml: a normal document parses");

        // A deeply nested preset in the folder is skipped by the browser's
        // listing and refused by a load.
        const auto file = temp.getChildFile ("Deep.ilanapreset");
        file.replaceWithText (deep);
        IlanaSynthAudioProcessor processor;
        check (! processor.loadPresetFromFile (file), "xml: a deeply nested preset file is refused, no crash");

        // The same text as host state: kept as unreadable, no crash.
        juce::MemoryBlock state;
        juce::AudioProcessor::copyXmlToBinary (juce::XmlElement ("ilanaSynth"), state);
        const auto text = deep.toStdString();
        juce::MemoryOutputStream out;
        out.writeInt (0x21324356);
        out.writeInt ((int) text.size());
        out.write (text.data(), text.size());
        out.writeByte (0);
        processor.setStateInformation (out.getData(), (int) out.getDataSize());
        check (true, "xml: deeply nested host state does not crash");

        // A normal state still round-trips.
        IlanaSynthAudioProcessor source;
        if (auto* p = source.getParameters()[3])
            p->setValueNotifyingHost (0.3f);
        juce::MemoryBlock saved;
        source.getStateInformation (saved);
        IlanaSynthAudioProcessor restored;
        restored.setStateInformation (saved.getData(), (int) saved.getSize());
        check (std::abs (restored.getParameters()[3]->getValue() - source.getParameters()[3]->getValue()) < 1.0e-6f,
               "xml: a normal state still loads");
    }

    // Wavetable data: a gzip bomb is refused without inflating it.
    {
        juce::MemoryOutputStream compressed;
        {
            juce::GZIPCompressorOutputStream zip (compressed, 9);
            juce::MemoryOutputStream raw;
            raw.writeInt (16);
            raw.writeFloat (1.0f);
            std::vector<char> zeros (1 << 20, 0);
            for (int i = 0; i < 64; ++i) // 64 MB of zeros after a 16-frame header
                zip.write (zeros.data(), zeros.size());
        }
        std::vector<std::vector<float>> frames;
        check (! WavetableDoc::decodeFrames (compressed.getMemoryBlock().toBase64Encoding(), frames),
               "wavetable: data far longer than any table is refused");
        const std::vector<std::vector<float>> real (3, std::vector<float> ((size_t) WavetableDoc::frameSize, 0.25f));
        check (WavetableDoc::decodeFrames (WavetableDoc::encodeFrames (real), frames) && frames.size() == 3
                   && std::abs (frames[2][100] - 0.25f) < 1.0e-3f,
               "wavetable: real embedded data still decodes");
    }

    // .kbm: numbers that wrap an int are refused, not thrown on.
    {
        Tuning::KeyboardMap map;
        juce::String error;
        const auto kbm = [] (const juce::String& size) { return size + "\n0\n127\n60\n69\n440.0\n12\n0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n"; };
        check (! Tuning::parseKeyboardMap (kbm ("2147483648"), map, error) && ! Tuning::parseKeyboardMap (kbm ("4294967295"), map, error),
               "tuning: a map size past 2^31 is refused");
        check (Tuning::parseKeyboardMap (kbm ("12"), map, error) && map.mapSize == 12, "tuning: a normal map still reads");
        Tuning::Scale scale;
        check (Tuning::parseScale ("big ratio\n1\n3486784401/2147483648\n", scale, error), "tuning: long ratios in a .scl still read");
    }

    // MIDI import: half a million overlapping notes on one key read in
    // linear time.
    {
        juce::MidiMessageSequence track;
        constexpr int notes = 200000;
        for (int i = 0; i < notes; ++i)
            track.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), i);
        for (int i = 0; i < notes; ++i)
            track.addEvent (juce::MidiMessage::noteOff (1, 60), notes + i);
        juce::MidiFile midi;
        midi.setTicksPerQuarterNote (96);
        midi.addTrack (track);
        const auto file = temp.getChildFile ("many.mid");
        {
            juce::FileOutputStream out (file);
            midi.writeTo (out);
        }
        Clip clip;
        juce::String error;
        const auto start = juce::Time::getMillisecondCounterHiRes();
        const auto ok = ClipState::importMidi (file, clip, error);
        const auto seconds = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
        check (ok && seconds < 5.0, "midi: 200k overlapping notes import quickly (" + juce::String (seconds, 2) + " s)");
    }

    // SFZ: one sample trimmed by many regions counts against the memory
    // budget; comments are stripped in one pass.
    {
        juce::AudioBuffer<float> audio (1, 48000 * 60); // 60 s
        audio.clear();
        const auto wav = temp.getChildFile ("long.wav");
        {
            juce::WavAudioFormat format;
            std::unique_ptr<juce::AudioFormatWriter> writer (format.createWriterFor (new juce::FileOutputStream (wav), 48000.0, 1, 16, {}, 0));
            writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
        }
        juce::String sfz;
        for (int i = 0; i < 2000; ++i)
            sfz << "<region> sample=long.wav offset=" << (i + 1) << " key=" << (i % 128) << "\n";
        sfz << juce::String::repeatedString ("/**/", 100000);
        const auto file = temp.getChildFile ("trim.sfz");
        file.replaceWithText (sfz);
        juce::String error;
        const auto start = juce::Time::getMillisecondCounterHiRes();
        const auto data = MultiSample::loadSfz (file, error);
        const auto seconds = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
        size_t bytes = 0;
        if (data != nullptr)
            for (const auto& zone : data->zones)
                bytes += (size_t) zone.data->getNumSamples() * (size_t) zone.data->getNumChannels() * sizeof (float);
        check (bytes <= (size_t) 800 * 1024 * 1024 && seconds < 30.0,
               "sfz: 2000 trims of one file stay inside the memory budget (" + juce::String ((double) bytes / 1.0e6, 0) + " MB, "
                   + juce::String (seconds, 1) + " s)");
    }

    temp.deleteRecursively();
}
