# Windows

On Windows, @atom/watcher uses the [`ReadDirectoryChangesW()`](https://msdn.microsoft.com/en-us/library/windows/desktop/aa365465%28v%3Dvs.85%29.aspx) call to monitor a directory for changes. ReadDirectoryChangesW is called in asynchronous mode with a completion routine.
Out-of-band command signalling is handled by an asynchronous procedure call scheduled with [`QueueUserAPC()`](https://msdn.microsoft.com/en-us/library/windows/desktop/ms684954%28v%3Dvs.85%29.aspx).

## ReadDirectoryChangesW oddities

The [`FILE_NOTIFY_INFORMATION` structure](https://msdn.microsoft.com/en-us/library/windows/desktop/aa364391%28v=vs.85%29.aspx) provided to the completion routine does not indicate whether the entry is a directory or a file. @atom/watcher uses [`GetFileAttributesW`](https://msdn.microsoft.com/en-us/library/windows/desktop/aa364944%28v=vs.85%29.aspx) to identify entry kinds, but leaves them as `"unknown"` for deletions.

If the filesystem does not support events and `ERROR_INVALID_FUNCTION` is returned, @atom/watcher will fall back to polling. However, depending on the filesystem, Windows may simply fail to deliver events.

## Known platform limits

A Windows process can open a maximum of 16,711,680 handles, so the number of watch roots is somewhat less than that :wink:
