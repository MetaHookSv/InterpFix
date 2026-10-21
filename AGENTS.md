# AGENTS.md

This file provides guidance and important rules working with code in this repository.

## C/C++ formatting

Owned C/C++ sources use the shared DiligentCore style from
[MetaHookSv/FormatValidation](https://github.com/MetaHookSv/FormatValidation), pinned
to clang-format 23.1.3. Setup, commands and the checked scope are recorded in
`memory/CppFormatting.md` (`CppFormatting`); the aggregator keeps the same note as
`metahooksv/cpp-formatting`. Do not hand-edit the generated `.clang-format`; change
the shared style. Keep MSVC inline assembly in explicit blocks.
