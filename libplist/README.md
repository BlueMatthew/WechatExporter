# libplist

*A small portable C library to handle Apple Property List files in binary or XML
format.*

![](https://github.com/libimobiledevice/libplist/workflows/build/badge.svg)

## Features

The project provides an interface to read and write plist files in binary or
XML format alongside a command-line utility named `plistutil`.

Some key features are:

- **Formats:** Supports binary and XML format
- **Utility:** Provides a `plistutil` utility for the command-line
- **Python:** Provides Cython based bindings for Python
- **Tested:** Uses fuzzing and data compliance tests
- **Efficient:** Lean library with performance and resources in mind

## Installation / Getting started

### Debian / Ubuntu Linux

First install all required dependencies and build tools:
```shell
sudo apt-get install \
	build-essential \
	checkinstall \
	git \
	autoconf \
	automake \
	libtool-bin
```

If you want to optionally build the documentation or Python bindings use:
```shell
sudo apt-get install \
	doxygen \
	cython
```

Then clone the actual project repository:
```shell
git clone https://github.com/libimobiledevice/libplist.git
cd libplist
```

Now you can build and install it:
```shell
./autogen.sh
make
sudo make install
```

## Usage

Then simply run:
```shell
plistutil -i foobar.plist -o output.plist
```

This converts the `foobar.plist` file to the opposite format, e.g. binary to
XML or vice versa, and outputs it to the `output.plist` file.

Please consult the usage information or manual page for a full documentation of
available command line options:
```shell
plistutil --help
man plistutil
```

## Contributing

We welcome contributions from anyone and are grateful for every pull request!

If you'd like to contribute, please fork the `master` branch, change, commit and
send a pull request for review. Once approved it can be merged into the main
code base.

If you plan to contribute larger changes or a major refactoring, please create a
ticket first to discuss the idea upfront to ensure less effort for everyone.

Please make sure your contribution adheres to:
* Try to follow the code style of the project
* Commit messages should describe the change well without being to short
* Try to split larger changes into individual commits of a common domain
* Use your real name and a valid email address for your commits

We are still working on the guidelines so bear with us!

## Links

* Homepage: https://libimobiledevice.org/
* Repository: https://git.libimobiledevice.org/libplist.git
* Repository (Mirror): https://github.com/libimobiledevice/libplist.git
* Issue Tracker: https://github.com/libimobiledevice/libplist/issues
* Mailing List: https://lists.libimobiledevice.org/mailman/listinfo/libimobiledevice-devel
* Twitter: https://twitter.com/libimobiledev

## License

This project is licensed under the [GNU Lesser General Public License v2.1](https://www.gnu.org/licenses/lgpl-2.1.en.html),
also included in the repository in the `COPYING` file.

## Credits

Apple, iPhone, iPad, iPod, iPod Touch, Apple TV, Apple Watch, Mac, iOS,
iPadOS, tvOS, watchOS, and macOS are trademarks of Apple Inc.

This project is an independent software library and has not been authorized,
sponsored, or otherwise approved by Apple Inc.

README Updated on: 2020-06-12
