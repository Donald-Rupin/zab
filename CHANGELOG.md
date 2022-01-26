# Changelog

All notable changes to this project are documented in this file.

See the [Keep a Changelog formatting convention](https://keepachangelog.com/en/1.0.0/) for a guide on contributing to this file, as well as their [example changelog](https://github.com/olivierlacan/keep-a-changelog/blob/master/CHANGELOG.md) to see what makes a good changelog file.

## Unreleased

## v0.0.0.2 2021/1/26

### Added

- This change log!
- `zab::for_each` for easily iterating over `zab::reusable_future` results. 
- `zab::first_of` for returning when the first future resolves. This is thread safe. 

### Changed
- Made a simpler echo server and moved current one to `logging_echo_server.cpp`
- All timers are now performed by `timing_service`.
- order_t is now relative (in nano-seconds) and no longer supports negative ordering.
- engine is now powered by io_uring
### Fixed
- Bug with event loop not picking the least busy loop in `kAnyThread`.
- Bug with timer service not resetting time check cadence. 

### Removed
- Event loop test since there is no ordering to test now.


