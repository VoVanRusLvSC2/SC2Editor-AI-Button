// Check Markdown parsing, links, code copying, and incomplete streaming markers.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include "../src/chat_markdown.h"
#include "../src/chat_outline.h"
int main() {
    auto spans = ParseMarkdown(
        L"**Title**\n- Item `code` [site](https://example.com)\n```galaxy\nhello();\n```\nunfinished **bold");
    bool bold = false, code = false, copy = false, link = false, bullet = false, unfinished = false;
    for (const auto &span : spans) {
        if (span.kind == MarkdownKind::Bold && span.text == L"Title")
            bold = true;
        if (span.kind == MarkdownKind::Code && span.text == L"hello();\n")
            code = true;
        if (span.kind == MarkdownKind::CopyCode && span.target == L"hello();\n")
            copy = true;
        if (span.kind == MarkdownKind::Link && span.text == L"site" && span.target == L"https://example.com")
            link = true;
        if (span.text.find(L"• Item") != std::wstring::npos)
            bullet = true;
        if (span.text.find(L"unfinished **bold") != std::wstring::npos)
            unfinished = true;
    }
    if (!bold || !code || !copy || !link || !bullet || !unfinished)
        return 1;
    auto unsafe = ParseMarkdown(L"[file](file:///C:/test)");
    for (const auto &span : unsafe)
        if (span.kind == MarkdownKind::Link)
            return 2;
    auto bare = ParseMarkdown(L"See https://example.com/path.");
    bool bareLink = false;
    for (const auto &span : bare)
        if (span.kind == MarkdownKind::Link && span.target == L"https://example.com/path")
            bareLink = true;
    if (!bareLink)
        return 4;
    auto incomplete = ParseMarkdown(L"```galaxy\nhello");
    std::wstring raw;
    for (const auto &span : incomplete)
        raw += span.text;
    if (raw != L"```galaxy\nhello")
        return 3;
    if (OutlineSnippet(L"First question\nmore detail", false) != L"First question" ||
        OutlineSnippet(L"Intro\n```galaxy\n# not a heading\n```\n## **Real heading**", true) !=
            L"Real heading" ||
        OutlineSnippet(L"Intro\n**Key point:**\nDetails", true) != L"Key point:" ||
        OutlineSnippet(L"```\nonly code\n```", true) != L"")
        return 5;
    std::puts("PASS: Markdown, links, code copy and incomplete stream markers");
    return 0;
}
