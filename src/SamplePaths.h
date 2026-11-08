#pragma once

#include <juce_core/juce_core.h>

// Sample and table paths are saved as the saving machine wrote them, so a set
// saved on Windows names C:\Users\ilana\...\kick.wav. JUCE on the Mac splits
// paths only on '/' (that whole string is one "file name" there), so these
// helpers read paths of either kind as text.
namespace SamplePaths
{
inline bool isSeparator (juce::juce_wchar c) { return c == '/' || c == '\\'; }

// The last part of a path, split on both slash kinds: "kick.wav".
inline juce::String fileNameOf (const juce::String& path)
{
    return path.substring (juce::jmax (path.lastIndexOfChar ('/'), path.lastIndexOfChar ('\\')) + 1);
}

inline juce::StringArray partsOf (const juce::String& path)
{
    juce::StringArray parts;
    juce::String part;
    for (auto p = path.getCharPointer(); ! p.isEmpty(); ++p)
    {
        if (isSeparator (*p))
        {
            if (part.isNotEmpty())
                parts.add (part);
            part.clear();
        }
        else
            part += juce::String::charToString (*p);
    }
    if (part.isNotEmpty())
        parts.add (part);
    return parts;
}

// The same place under another machine's home folder: what follows
// "Users\<name>" (Windows, macOS) or "home/<name>" (Linux), put under `home`
// with home's own separator. A path under OneDrive (where Windows often keeps
// Documents) also gives the place without that folder, as a second choice.
// Empty when the path is not under a home folder.
inline juce::StringArray underHome (const juce::String& path, const juce::String& home)
{
    const auto parts = partsOf (path);
    auto start = -1;
    for (int i = 0; i + 2 < parts.size() && start < 0; ++i)
        if (parts[i].equalsIgnoreCase ("Users") || parts[i] == "home")
            start = i + 2;

    juce::StringArray out;
    if (start < 0 || home.isEmpty())
        return out;

    const auto separator = home.containsChar ('\\') && ! home.containsChar ('/') ? juce::String ("\\") : juce::String ("/");
    const auto root = isSeparator (home.getLastCharacter()) ? home.dropLastCharacters (1) : home;
    const auto join = [&] (int from)
    {
        juce::StringArray rest;
        for (int i = from; i < parts.size(); ++i)
            rest.add (parts[i]);
        return root + separator + rest.joinIntoString (separator);
    };
    out.add (join (start));
    if (parts[start].startsWithIgnoreCase ("OneDrive") && start + 1 < parts.size())
        out.add (join (start + 1));
    return out;
}

// The file a saved path names on this machine: the path itself, or the same
// place under this machine's home folder (a set moved between Windows and
// the Mac). juce::File() when neither exists.
inline juce::File findFile (const juce::String& path)
{
    if (juce::File::isAbsolutePath (path))
        if (const juce::File file (path); file.existsAsFile())
            return file;
    const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName();
    for (const auto& candidate : underHome (path, home))
        if (juce::File::isAbsolutePath (candidate))
            if (const juce::File file (candidate); file.existsAsFile())
                return file;
    return {};
}

// The folder to start a Locate in: the path's own folder, or the same place
// under this home folder, if there.
inline juce::File folderOf (const juce::String& path)
{
    const auto parent = [] (const juce::String& text)
    {
        const auto cut = juce::jmax (text.lastIndexOfChar ('/'), text.lastIndexOfChar ('\\'));
        return cut > 0 ? text.substring (0, cut) : juce::String();
    };
    if (const auto own = parent (path); juce::File::isAbsolutePath (own) && juce::File (own).isDirectory())
        return juce::File (own);
    const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName();
    for (const auto& candidate : underHome (path, home))
        if (const auto folder = parent (candidate); juce::File::isAbsolutePath (folder) && juce::File (folder).isDirectory())
            return juce::File (folder);
    return {};
}
} // namespace SamplePaths
