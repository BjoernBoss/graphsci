# Rendering Graphs in Terminal using C++
![C++](https://img.shields.io/badge/language-c%2B%2B20-blue?style=flat-square)
[![License](https://img.shields.io/badge/license-BSD--3--Clause-brightgreen?style=flat-square)](LICENSE.txt)

Header only library written in `C++20` to render graphs containing nodes and directed or undirected edges to strings. Simply include [`graphsci.h`](graphsci.h) to add support for it.

The constructed graph is defined by edges between nodes. The order of the nodes will not be modified and will be taken as provided by the caller. The library will then choose an ordering for the edges and render it to a string representation, while optimizing for reduced size and visual complexity.

This preserved node ordering allows the library to be used to represent complex historic flows, but still allows the use for any graphs.

## Using the library
This library is a header only library. Simply clone the repository, and include `<graphsci/graphsci.h>`.

    $ git clone https://github.com/BjoernBoss/graphsci.git

## Library Usage

The library supports edges between any two nodes, which can conditionally have arrow heads on the ends of edges. But there can at most be one edge between any two nodes.

The graphs are built edge-by-edge:
```C++
ascg::Builder builder{};
builder.connect(0, 3);
builder.connect(0, 4);
builder.connect(0, 5, true, true);
builder.connect(1, 1);
builder.connect(1, 6);
builder.connect(2, 4, false, true);
builder.connect(2, 6);
builder.connect(4, 4, true);
builder.connect(5, 6);
```

Finally, the graph can be rendered using the finalize method:
```C++
const ascg::Graph graph = builder.finalize(theme);
for (const auto& row : graph.rows)
	std::cout << row << std::endl;
```

## Render Themes

The algorithm renders the graph to string. The chacter set to be used can be customized via `ascg::Theme`. This also allows to adjust the width of the rendered graph.

The library comes with some predefined themes and theme builders:
```
  ascg::AsciiTheme:       ascg::UnicodeTheme:     ascg::UnicodeTheme:
                           - Rounded Corners       - Heavy Lines
                                                   - Heavy Nodes

    #####              |    ■■■■■              |    █████
    A | '---------,    |    ▲ │ ╰─────────╮    |    ▲ ┃ ┗━━━━━━━━━┓
  ,-' | ,-,       |    |  ╭─╯ │ ╭─╮       │    |  ┏━┛ ┃ ┏━┓       ┃
  |   | #######   |    |  │   │ ■■■■■■■   │    |  ┃   ┃ ███████   ┃
  | ,-|-' | | '---|-,  |  │ ╭─│─╯ │ │ ╰───│─╮  |  ┃ ┏━┃━┛ ┃ ┃ ┗━━━┃━┓
  | | |   | ###   | |  |  │ │ │   │ ■■■   │ │  |  ┃ ┃ ┃   ┃ ███   ┃ ┃
  | | ##### | '-, | |  |  │ │ ■■■■■ │ ╰─╮ │ │  |  ┃ ┃ █████ ┃ ┗━┓ ┃ ┃
  | | | A '-|-, V | |  |  │ │ │ ▲ ╰─│─╮ ▼ │ │  |  ┃ ┃ ┃ ▲ ┗━┃━┓ ▼ ┃ ┃
  | | | | ,-' ##### |  |  │ │ │ │ ╭─╯ ■■■■■ │  |  ┃ ┃ ┃ ┃ ┏━┛ █████ ┃
  V | | | |   A |   |  |  ▼ │ │ │ │   ▲ │   │  |  ▼ ┃ ┃ ┃ ┃   ▲ ┃   ┃
  ##### | |   '-'   |  |  ■■■■■ │ │   ╰─╯   │  |  █████ ┃ ┃   ┗━┛   ┃
      | | | ,-------'  |      │ │ │ ╭───────╯  |      ┃ ┃ ┃ ┏━━━━━━━┛
      #######          |      ■■■■■■■          |      ███████
```

The `ascg::AsciiTheme` is the default theme. Besides, there exists the `ascg::ExtendedTheme`, which renders the graph to the `CP437`. Finally, the `ascg::UnicodeTheme` method allows to build various different styles based on unicode characters.

## Algorithm Performance

The algorithms do not guarantee to select the perfect ordering of the edges. Rather, they strive for a best-effort solution. 

Given that the rendering and visual complexity reduction can become quite time intensive, the `finalize` method also takes a stop-callback. The algorithm will continue optimization until a stable end has been reached, or until the stop-callback returns true, in which case the currently best representation will be returned.

```C++
/* ensure that the graph runs at most two seconds */
auto start = std::chrono::high_resolution_clock::now();
const auto graph = builder.finalize([start] { 
	auto elapsed = (std::chrono::high_resolution_clock::now() - start);
	return elapsed >= std::chrono::seconds(2);
});
```

## Example of a K10 Graph

The following example show a K10 graph without self-loops, and the default `ascg::UnicodeTheme`:
```
                  ■■■■■■■■■■■■■■■■■
        ┌─────────┘ │ │ │ │ │ │ │ └───────────────────┐
┌───────│───────────┘ │ │ │ │ │ └───────────────────┐ │
│       │       ┌─────┘ │ │ │ └───────────────────┐ │ │
│       │       │       │ │ └───────────────┐     │ │ │
│       │       │       │ ■■■■■■■■■■■■■■■   │     │ │ │
│ ┌─────│───────│───────│─┘ │ │ │ │ │ │ │   │     │ │ │
│ │     │ ┌─────│───────│───┘ │ │ │ │ │ └───│───┐ │ │ │
│ │     │ │     │ ┌─────│─────┘ │ │ │ └───┐ │   │ │ │ │
│ │     │ │     │ │ ■■■■■■■■■■■■■ │ │     │ │   │ │ │ │
│ │ ┌───│─│─────│─│─┘ │ │ │ │ │ └─│─│─────│─│─┐ │ │ │ │
│ │ │   │ │ ┌───│─│───┘ │ │ │ └───│─│───┐ │ │ │ │ │ │ │
│ │ │   │ │ │ ■■■■■■■■■■■ │ └───┐ │ │   │ │ │ │ │ │ │ │
│ │ │ ┌─│─│─│─┘ │ │ │ │ └─│─────│─│─│─┐ │ │ │ │ │ │ │ │
│ │ │ │ ■■■■■■■■■ │ │ └───│───┐ │ │ │ │ │ │ │ │ │ │ │ │
│ │ │ │ │ │ │ │ │ │ └─────│─┐ │ │ │ │ │ │ │ │ │ │ │ │ │
■■■■■■■■■ │ │ │ │ └─┐ ┌───┘ │ │ │ │ │ │ │ │ │ │ │ │ │ │
  │ │ │ │ │ │ │ └─┐ │ │ ┌───│─│─│─┘ │ │ │ │ │ │ │ │ │ │
  │ │ │ └─│─│─│─┐ │ │ │ │ ┌─│─│─│───│─│─│─│─┘ │ │ │ │ │
  │ │ │   │ │ │ ■■■■■■■■■■■ │ │ │ ┌─┘ │ │ │   │ │ │ │ │
  │ │ │   │ │ │ │   │ ┌───│─┘ │ │ │ ┌─│─│─│───│─│─┘ │ │
  │ │ │   │ │ │ │   │ │   └─┐ │ │ │ │ │ │ │   │ │   │ │
  │ │ │   │ │ └─│───│─│───┐ │ │ │ │ │ │ │ │   │ │   │ │
  │ │ └───│─│───│───│─│─┐ │ │ │ │ │ │ │ │ │   │ │   │ │
  │ │     │ │   │   │ │ ■■■■■■■■■■■■■ │ │ │   │ │   │ │
  │ │     │ └───│─┐ │ │ │ ┌─────│─────│─┘ │   │ │   │ │
  │ │     │   ┌─┘ │ │ │ │ │ ┌───│─────│───┘   │ │   │ │
  │ └─────│───│─┐ │ │ │ │ │ │ ┌─│─────│───────│─│───┘ │
  │       │   │ ■■■■■■■■■■■■■■■ │ ┌───┘       │ │     │
  │       │   └─────────────┐ │ │ │ ┌─────────┘ │     │
  │       └───────────────┐ │ │ │ │ │ ┌─────────┘     │
  └─────────────────────┐ │ │ │ │ │ │ │ ┌─────────────┘
                        ■■■■■■■■■■■■■■■■■
```
