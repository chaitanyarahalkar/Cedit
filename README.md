# Cedit
[![Build Status](https://travis-ci.org/chaitanyarahalkar/Cedit.svg?branch=master)](https://travis-ci.org/chaitanyarahalkar/Cedit)
[![GitHub license](https://img.shields.io/github/license/chaitanyarahalkar/Cedit)](https://github.com/chaitanyarahalkar/Cedit/blob/master/LICENSE)

![Cedit](cedit.png)

## What is Cedit? 

TL;DR - A minimalistic text editor for terminal fanatics. 

Cedit is a zero-dependency text editor for the terminal. Cedit is similar to other popular editors like Vim, Emacs, Nano etc. It is designed with the philosophy of minimalism and simplicity. It requires no external libraries (Not even [curses](https://en.wikipedia.org/wiki/Curses_(programming_library))) or dependencies for installation. It is just a single-file C program that caters all your editing needs. Cedit can be compiled on any platform having GCC or the Clang C compiler. The idea behing Cedit is to eliminate the key-binding complications of Vim and port the existing key-bindings of your familiar GUI based editors to CLI. A statically compiled binary can allow it to run even on devices without the standard C library. This has facilitated the text editor to be used in embedded devices, routers, PoS terminals, which have very limited disk space. Cedit conforms to [VT100](https://vt100.net) key and escape sequence bindings. 

## Features

Cedit is currently in beta stage. New features would be added in future stable releases.
Cedit supports:

- Syntax highlighting supported for over 20 programming languages
- Defining your own syntax for highlighting code blocks
- Incremental string searching

Improvements to be made in future releases:

- Regex pattern searching
- Soft indents
- Line numbers
- Reading from configuration file (.rc file)
- Auto indentation

## Building from Source

The project requires a standard C compiler like GCC or Clang and project building tool - [Make](https://www.gnu.org/software/make/)

```bash

build@editor$: make

```

will produce the editor binary (cedit) with a single dynamically linked library - (libc.so.6 for Linux & libSystem.B.dylib for MacOS)


## Author

 **Chaitanya Rahalkar**

* Twitter: [@chairahalkar](https://twitter.com/chairahalkar)
* Github: [@chaitanyarahalkar](https://github.com/chaitanyarahalkar)

#### Contributing

Contributions, issues and feature requests are welcome!<br/>Feel free to check [issues page](https://github.com/chaitanyarahalkar/Cedit/issues).

#### Show your support

Give a ⭐️ if this project helped you!

#### License

Copyright © 2020 [Chaitanya Rahalkar](https://github.com/chaitanyarahalkar).<br />
This project is [Apache 2.0](https://github.com/chaitanyarahalkar/Cedit/blob/master/LICENSE) licensed.
