# Getting Started on macOS #

## Getting the source ##

Fork the C driver repository: https://github.com/mongodb/mongo-c-driver

Clone the fork somewhere memorable. replace `<username>`):

```
mkdir ~/code
cd ~/code
git clone git@github.com:<username>/mongo-c-driver.git
```

Add the remote for `upstream` pointing to the main repo.

```
git remote add upstream git@github.com:mongodb/mongo-c-driver.git
```

Now `origin` points to your fork, and `upstream` points to the main repo. You can verify this by running `git remote -v`.

Create a global gitignore file file in your home directory (`~/.gitignore_global`) with the following contents.
```
cmake-build*/
.idea/
codereview.rc
*.pyc
.DS_STORE
.vscode/
```

Register it:
```
git config --global core.excludesfile ~/.gitignore_global
```

That ignores common files created by IDEs or during build that git should always ignore.

## Building ##

On macOS, install development tools.

```
xcode-select --install
```

Use `cmake` to configure.
```
cd ~/code/mongo-c-driver
cmake -Bcmake-build -DENABLE_MAINTAINER_FLAGS=ON -DCMAKE_C_FLAGS="-fsanitize=address -DBSON_MEMCHECK -Werror"
```

That generates system dependent build files and configuration in the directory `cmake-build`. The flags in the cmake command are as follows:

- `-Bcmake-build` says to configure and build everything in the `cmake-build` directory. If this is not specified, build files get intermingled with source files, which can make it harder to separate builds for multiple configurations. `cmake-build` should be ignored by git. It can be safely deleted to get a completely fresh rebuild (though a fresh rebuild takes longer).
- `-DENABLE_MAINTAINER_FLAGS` enables additional compiler warnings which we build with in testing.
- `-DCMAKE_C_FLAGS` are additional compiler flags.
    - `-fsanitize=address` builds with clang's [Address Sanitizer](https://clang.llvm.org/docs/AddressSanitizer.html). This detects common C bugs (leaks, use-after-free, double frees) and gives stack traces to identify the cause.
    - `-DBSON_MEMCHECK` detects common leaks when using a `bson_t` in libbson.

Now, you can build executables. For example:

```
cmake --build cmake-build --target example-client
```

The `example-client` is an example executable that connects to MongoDB and prints out the contents of the `test.test` collection. Run it with:

```
./cmake-build/src/libmongoc/example-client
```

If mongod is running, and there are documents in the `test.test` collection, you should see a list of those documents printed.

Use `cmake --build cmake-build --target help` to see a list of all targets. Omit `--target` to build everything.


## Formatting ##

The C driver formats code using clang-format. Install clang-format 4.0.1. We need exactly 4.0.1, not the latest, to ensure we get the same code layout every time:

```
wget http://releases.llvm.org/4.0.1/clang+llvm-4.0.1-x86_64-apple-darwin.tar.xz
tar -xf clang+llvm-4.0.1-x86_64-apple-darwin.tar.xz
# Put clang-format somewhere accessible
mv clang+llvm-4.0.1-x86_64-apple-macosx10.9.0/ ~/llvm-4.0.1
```

Once installed, from the mongo-c-driver you should be able to clang-format a specific file with:
```
~/llvm-4.0.1/bin/clang-format -style=file -i ./src/libmongoc/src/mongoc/mongoc-cursor.c
```

## Testing locally ##

To build the test runner:

```
cmake --build cmake-build --target test-libmongoc
```

`test-libmongoc` runs tests depending on the environment. To run the majority of tests, start a two node replica set. This can be done with `mlaunch` as follows:

```
cd ~/bin
mlaunch init --replicaset --nodes=2 --setParameter enableTestCommands=1 --dir replset
```

The `--setParameter enableTestCommands=1` enables [failpoint commands](https://github.com/mongodb/mongo/wiki/The-%22failCommand%22-fail-point) on the server that tests rely on.

Run tests locally with:

```
./cmake-build/src/libmongoc/test-libmongoc --no-fork -d -l "*"
```

- `--no-fork` prevents each test from running in a separate process. This makes tests run a lot faster. Though a test failure terminates test-libmongoc.
- `-d` prints out additional debug information.
- `-l` selects a subset of tests. Omit it, or use `*` to select all tests.

Use `./cmake-build/src/libmongoc/test-libmongoc --help` to see all options.

`test-libmongoc` can be configured with additional environment variables. That is documented in [CONTRIBUTING.md](https://github.com/mongodb/mongo-c-driver/blob/master/CONTRIBUTING.md).


## Working on a Ticket ##

Get a ticket assigned to you in Jira. (e.g. `CDRIVER-XXXX`).

Create a branch in your fork of the repo for working on the ticket.

```
git checkout -b myticket.CDRIVER-XXXX
```

If appropriate, write a test of the new behavior. Run the test with:
```
./cmake-build/src/libmongoc/test-libmongoc --no-fork -d -l /name/of/test
```

The new test should fail. Make the necessary change to get the test passing. Run other tests locally if appropriate.

Format with "clang-format".

Commit frequently to `myticket.CDRIVER-XXXX`. See your changes with `git diff master..myticket.CDRIVER-XXXX`.

Submit a pull-request.
```
git push origin myticket.CDRIVER-XXXX
```

A URL should be printed, navigate to it, and create the pull request.

Request reviewers on the pull request.

Copy-paste a link of the pull request as a comment in the Jira ticket. E.g. navigate to https://jira.mongodb.org/browse/CDRIVER-XXXX and add a comment like "PR: https://github.com/mongodb/mongo-c-driver/pull/XXX".

Once the PR is approved, "Squash and Merge" it to create one commit. The commit message follows this pattern:
```
CDRIVER-XXXX all lowercase message no final period
```

Close the ticket on Jira. Ensure a `fixVersion` is set to the release that this fix will be applied to.

# Further Reading #

- [Tutorial](http://mongoc.org/libmongoc/current/tutorial.html) for using the C driver.
- [Aids for Debugging](http://mongoc.org/libmongoc/current/debugging.html)