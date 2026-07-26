# niniBUS Visualizer

This folder contains a native macOS application for explaining niniBUS.
It does not use a browser, local server, Python, or an external runtime.

## Run

Double-click:

```text
niniBUS Visualizer.app
```

The app lets you:

- call the four public APIs with their real arguments:
  `createLane`, `publish`, `subscribe`, and `receive`;
- watch API calls construct and update the `lanes_` table;
- select a table row and inspect its mapped `Lane` object;
- follow the Lane's `content_` member into a circular `cfifo` ring;
- inspect physical slots, logical sequence IDs, head, tail, size, and credit;
- register and inspect subscriber cursors;
- publish messages and inspect sequence IDs and credit;
- receive independently for each subscriber;
- watch physical circular-buffer slots, head, tail, size, and capacity;
- trigger write-pressure reclamation and inspect skipped-message reporting;
- run a guided demonstration.

## Rebuild

On macOS with Xcode command-line tools:

```sh
cd visualization
make
```
