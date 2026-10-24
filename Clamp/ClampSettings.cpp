#include "ClampSettings.h"

#include "config.h"
#include "IPlugPaths.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace
{
/** One section of an INI file. Keys keep the order of the file. */
struct IniSection
{
  std::string name;
  std::vector<std::pair<std::string, std::string>> values;
};

std::string Trim(const std::string& str)
{
  const auto first = str.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return "";
  const auto last = str.find_last_not_of(" \t\r\n");
  return str.substr(first, last - first + 1);
}

std::string ToLower(std::string str)
{
  std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return str;
}

bool IsComment(const std::string& line)
{
  return !line.empty() && (line[0] == ';' || line[0] == '#');
}

bool IsSection(const std::string& line)
{
  return line.size() >= 2 && line.front() == '[' && line.back() == ']';
}

std::string SectionName(const std::string& line)
{
  return ToLower(Trim(line.substr(1, line.size() - 2)));
}

/** Splits a "key=value" line. Section and key names are case-insensitive, as in mINI. */
bool SplitKeyValue(const std::string& line, std::string& key, std::string& value)
{
  const auto eq = line.find('=');
  if (eq == std::string::npos)
    return false;
  key = ToLower(Trim(line.substr(0, eq)));
  value = Trim(line.substr(eq + 1));
  return !key.empty();
}

/** Creates the folder and its parents, like std::filesystem::create_directories. */
bool CreateDirectories(const std::string& path)
{
  for (size_t pos = 1; pos <= path.size(); pos++)
  {
    if (pos == path.size() || path[pos] == '/')
    {
      const std::string part = path.substr(0, pos);
      if (mkdir(part.c_str(), 0755) != 0 && errno != EEXIST)
        return false;
    }
  }
  return true;
}

/** ~/Library/Application Support/clamp-mac/Clamp/Settings.ini. Returns false if the folder cannot be made. */
bool SettingsPath(std::string& path)
{
  WDL_String appSupport;
  iplug::AppSupportPath(appSupport);
  if (!appSupport.GetLength())
    return false;

  const std::string folder = std::string(appSupport.Get()) + "/" BUNDLE_MFR "/" BUNDLE_NAME;
  if (!CreateDirectories(folder))
    return false;

  path = folder + "/Settings.ini";
  return true;
}

bool ReadLines(const std::string& path, std::vector<std::string>& lines)
{
  std::ifstream file(path);
  if (!file)
    return false;
  std::string line;
  while (std::getline(file, line))
  {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    lines.push_back(line);
  }
  return true;
}

std::vector<IniSection> ParseIni(const std::vector<std::string>& lines)
{
  std::vector<IniSection> sections;
  for (const std::string& raw : lines)
  {
    const std::string line = Trim(raw);
    std::string key, value;
    if (line.empty() || IsComment(line))
      continue;
    if (IsSection(line))
      sections.push_back({SectionName(line), {}});
    else if (!sections.empty() && SplitKeyValue(line, key, value))
      sections.back().values.emplace_back(key, value);
  }
  return sections;
}

const std::string* FindValue(const std::vector<IniSection>& sections, const std::string& section, const std::string& key)
{
  for (const IniSection& s : sections)
  {
    if (s.name != section)
      continue;
    for (const auto& kv : s.values)
    {
      if (kv.first == key)
        return &kv.second;
    }
  }
  return nullptr;
}

/** Writes the lines with "\n" between them and no final newline, like the mINI generator. */
bool WriteLines(const std::string& path, const std::vector<std::string>& lines)
{
  std::ofstream file(path, std::ios::trunc);
  if (!file)
    return false;
  for (size_t i = 0; i < lines.size(); i++)
  {
    if (i > 0)
      file << "\n";
    file << lines[i];
  }
  return static_cast<bool>(file);
}

/** Sets key=value in the INI lines. It updates the key in place, adds it at the end of its section,
 * or adds the section at the end of the file. */
void SetValue(std::vector<std::string>& lines, const std::string& section, const std::string& key, const std::string& value)
{
  std::string current;
  int sectionEnd = -1;

  for (size_t i = 0; i < lines.size(); i++)
  {
    const std::string line = Trim(lines[i]);
    std::string k, v;
    if (IsSection(line))
    {
      current = SectionName(line);
      if (current == section)
        sectionEnd = static_cast<int>(i);
    }
    else if (current == section && !line.empty() && !IsComment(line) && SplitKeyValue(line, k, v))
    {
      if (k == key)
      {
        lines[i] = key + "=" + value;
        return;
      }
      sectionEnd = static_cast<int>(i);
    }
  }

  if (sectionEnd >= 0)
  {
    lines.insert(lines.begin() + sectionEnd + 1, key + "=" + value);
  }
  else
  {
    lines.push_back("[" + section + "]");
    lines.push_back(key + "=" + value);
  }
}
} // namespace

void ReadClampSettings(ClampSettings& settings)
{
  std::string path;
  if (!SettingsPath(path))
    return;

  std::vector<std::string> lines;
  ReadLines(path, lines);
  const std::vector<IniSection> sections = ParseIni(lines);

  // No sections: the original keeps its in-code defaults and writes a default
  // file. That file's values differ from the in-code defaults.
  if (sections.empty())
  {
    WriteLines(path, {"[graphics]", "scale=1.0", "color=1", "[util]", "tooltips=0"});
    return;
  }

  // The original parses with std::stof and std::stoi. A missing or malformed
  // value throws there; the port keeps that value's default instead.
  if (const std::string* pScale = FindValue(sections, "graphics", "scale"))
  {
    try { settings.scale = std::max(0.5f, std::stof(*pScale)); } catch (...) {}
  }

  // The original clamps the colour to 0..5 although the palette has 8 entries.
  if (const std::string* pColor = FindValue(sections, "graphics", "color"))
  {
    try { settings.color = std::clamp(std::stoi(*pColor), 0, 5); } catch (...) {}
  }

  if (const std::string* pTooltips = FindValue(sections, "util", "tooltips"))
  {
    try { settings.tooltips = std::clamp(std::stoi(*pTooltips), 0, 1); } catch (...) {}
  }
}

void WriteClampSettings(const ClampSettings& settings)
{
  std::string path;
  if (!SettingsPath(path))
    return;

  std::vector<std::string> lines;
  ReadLines(path, lines);

  // The original writes snprintf("%f") cut to 4 characters, for example "1.00".
  char scale[32];
  std::snprintf(scale, sizeof(scale), "%f", settings.scale);
  SetValue(lines, "graphics", "scale", std::string(scale).substr(0, 4));

  if (settings.color >= 0)
    SetValue(lines, "graphics", "color", std::to_string(settings.color));

  SetValue(lines, "util", "tooltips", std::to_string(settings.tooltips ? 1 : 0));

  WriteLines(path, lines);
}
