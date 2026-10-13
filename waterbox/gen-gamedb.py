#!/usr/bin/env python3
# Turns PCSX2's per-title database into a table the core carries inside itself.
#
# WHAT THIS IS FOR. bin/resources/GameIndex.yaml is where PCSX2 keeps what it
# knows about individual games: the clamp and round modes a title's floating
# point needs, the game fixes it needs, the GS hardware fixes it needs. Without
# it Final Fantasy X's bosses face the wrong way in some fights (SLUS-20312 asks
# for eeClampMode 3, and says so in its own comment), and several thousand other
# titles lose their own corrections in their own ways.
#
# Upstream reads that file at runtime out of its resources directory
# (GameDatabase::initDatabase). A core has no resources directory: its file
# system is whatever the project mounted, and nobody mounts an emulator's
# database. So it is compiled in, the same way the shaders are
# (waterbox/gen-shaders.py), and waterbox/game-database.cpp reads it from here.
#
# WHY IT IS NOT THE YAML ITSELF. Reading the file verbatim would mean carrying a
# YAML parser into the sandbox, and PCSX2 does not vendor one - it takes rapidyaml
# from the system, which a guest build has no way to get. So the tree is
# flattened here, at build time, into lines the core reads with no parser at all.
# The KEYS stay upstream's, spelled exactly as the yaml spells them, because the
# core resolves every one of them through upstream's own lookups
# (parseHWFixName, ParseSpeedHackName, GetGameFixName): a pin bump that renames a
# fix must be answered by upstream's table, not by a copy of it living here.
#
# THE PATCHES ARE CARRIED TOO (user-decided, 2026-10-13: "On, as on upstream").
# A title's "patches" are PCSX2's own corrections to the game's code - Black's
# fixes the collisions the game gets wrong on an emulator - and a PCSX2 user
# has them without asking. They were left out here once, on the grounds that a
# movie must not have the game rewritten behind its back; but they are compiled
# in like the rest of this file, so the package a movie cites pins them exactly
# as it pins the clamp modes. A patch is "patches/<crc>" and its text with
# every newline, tab and backslash escaped (\n, \t, \\), because a field here
# is one line. A "dynaPatch" is a pattern of instructions and what to put in
# its place, "offset:value,.../offset:value,..." in decimal.
#
# WHAT IS LEFT OUT, and why each one:
#   name-sort, name-en    a game list's business. This core has no game list.
#   compat                a compatibility rating shown in a UI there isn't one of.
#   region                nothing in the machine reads it.
# "name" is kept, for one reason: it is what the log says it matched, and a
# database that loaded the wrong entry should be readable rather than deduced.
#
# EVERY OTHER KEY IS AN ERROR. A pin bump that adds a key nobody here has heard
# of stops the build and names it, rather than dropping it quietly - dropping it
# quietly is exactly how this whole database came to be missing for months.
#
# Usage: gen-gamedb.py <GameIndex.yaml> <output .cpp>
import re
import sys

import yaml

# Nested maps: group -> the keys under it are emitted as "group/key value".
MAP_GROUPS = ("roundModes", "clampModes", "speedHacks", "gsHWFixes")
# Sequences: the key repeats, once per element.
SEQ_GROUPS = ("gameFixes", "memcardFilters")
# Carried as-is.
SCALARS = ("name",)
# Known, and deliberately not carried (see the header).
DROPPED = ("name-sort", "name-en", "region", "compat")
# Carried, each in its own shape (see the header).
PATCHES = ("patches", "dynaPatches")

src_path, out_path = sys.argv[1], sys.argv[2]

with open(src_path, "rb") as f:
    raw = f.read()
# resolve_anchors: upstream parses this file with anchors resolved, and the
# database uses them (Gran Turismo 4's clamp modes are an alias). PyYAML
# resolves them on the way in, so an alias arrives here as the value it names.

class Loader(yaml.CSafeLoader):
    """A key is text. A patch is filed under the checksum of its program, eight
    hexadecimal digits, and one made of digits only (SLES-50580's 33611180)
    would arrive as a number - or, with a leading zero, as another number, read
    in base eight. Upstream reads keys as text, and so does this."""

    def construct_mapping(self, node, deep=False):
        if isinstance(node, yaml.MappingNode):
            for key_node, _ in node.value:
                if isinstance(key_node, yaml.ScalarNode) and key_node.tag in (
                        "tag:yaml.org,2002:int", "tag:yaml.org,2002:float"):
                    key_node.tag = "tag:yaml.org,2002:str"
        return super().construct_mapping(node, deep)


db = yaml.load(raw.decode("utf-8"), Loader=Loader)

lines = []
entries = 0
fields = 0
patches = 0


def emit(key, value):
    global fields
    fields += 1
    if value is None:
        # A key with no value. Upstream reads that as 1 (GameDatabase.cpp's
        # n.has_val() ? ... : 1), and a line with no tab says the same thing.
        lines.append("\t" + key)
    else:
        text = str(value)
        if "\n" in text or "\t" in text:
            sys.exit("%s: value of '%s' contains a tab or a newline" % (serial, key))
        lines.append("\t" + key + "\t" + text)


for serial, entry in db.items():
    if not isinstance(entry, dict):
        # Upstream skips a root child that is not a map (parseAndInsert is only
        # called for n.is_map()), and so does this.
        continue
    for key in entry:
        if key not in SCALARS and key not in MAP_GROUPS and key not in SEQ_GROUPS \
                and key not in DROPPED and key not in PATCHES:
            sys.exit("%s: unknown GameIndex.yaml key '%s'. Upstream has grown a "
                     "field this core has never been taught. Decide what it means "
                     "here, in waterbox/gen-gamedb.py, and say so - do not let it "
                     "be dropped quietly." % (serial, key))
    if "\n" in serial or "\t" in serial:
        sys.exit("serial '%s' contains a tab or a newline" % serial)
    entries += 1
    lines.append(serial)
    for key in SCALARS:
        if key in entry and entry[key] is not None:
            emit(key, entry[key])
    for group in MAP_GROUPS:
        node = entry.get(group)
        if isinstance(node, dict):
            for key, value in node.items():
                emit(group + "/" + key, value)
    for group in SEQ_GROUPS:
        node = entry.get(group)
        if isinstance(node, list):
            for value in node:
                if value is not None:
                    emit(group, value)
    # The patches, by the checksum of the program they are for ("default" is
    # for any). Upstream takes a patch with no "content" as an empty one, and
    # so does this: the line has a tab and nothing after it.
    node = entry.get("patches")
    if isinstance(node, dict):
        for crc, patch in node.items():
            if not isinstance(crc, str) or not re.fullmatch(r"default|[0-9A-Fa-f]{1,8}", crc, re.I):
                sys.exit("%s: a patch's checksum is %r, which is not 'default' or hexadecimal text" % (serial, crc))
            content = patch.get("content") if isinstance(patch, dict) else None
            text = "" if content is None else str(content)
            emit("patches/" + crc, text.replace("\\", "\\\\").replace("\n", "\\n").replace("\t", "\\t"))
            patches += 1
    node = entry.get("dynaPatches")
    if isinstance(node, list):
        for patch in node:
            def side(name):
                rows = patch.get(name) if isinstance(patch, dict) else None
                if not isinstance(rows, list):
                    return ""
                for row in rows:
                    if not isinstance(row.get("offset"), int) or not isinstance(row.get("value"), int):
                        sys.exit("%s: a dynaPatch row is %r" % (serial, row))
                return ",".join("%d:%d" % (row["offset"], row["value"]) for row in rows)
            # upstream reads the replacement only where there is a pattern
            pattern = side("pattern")
            emit("dynaPatch", pattern + "/" + (side("replacement") if pattern else ""))
            patches += 1

blob = "\n".join(lines) + "\n"

# A raw string literal, cut into pieces the compiler is comfortable with;
# adjacent literals are one array by the time anything reads it. The delimiter
# is one no game's name can contain, and it is checked rather than assumed.
DELIM = "CHIMERAGAMEDB"
if ')' + DELIM + '"' in blob:
    sys.exit("the blob contains the raw-string delimiter")
CHUNK = 16384
chunks = [blob[i:i + CHUNK] for i in range(0, len(blob), CHUNK)]

out = [
    "/* GENERATED by waterbox/gen-gamedb.py - do not edit.",
    " *",
    " * PCSX2's per-title database, compiled into the core. See the script for",
    " * what it carries, what it leaves out, and why.",
    " */",
    "#include <cstddef>",
    "",
    "namespace",
    "{",
    "const char kGameDB[] =",
]
for chunk in chunks:
    out.append('\tR"%s(%s)%s"' % (DELIM, chunk, DELIM))
out.append("\t;")
out.append("} // namespace")
out.append("")
out.append("const char *ChimeraGameDB(size_t *size)")
out.append("{")
out.append("\tif (size != nullptr)")
out.append("\t\t*size = sizeof(kGameDB) - 1;")
out.append("\treturn kGameDB;")
out.append("}")
out.append("")

with open(out_path, "w") as f:
    f.write("\n".join(out))

print("%d titles, %d fields (%d of them patches), %d bytes" % (entries, fields, patches, len(blob)), file=sys.stderr)
