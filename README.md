# WHDLoadContainers - proposed addition for WHDLoad

This is work in progress proposal to add support for loading games directly from archives to WHDLoad. This is where the project name is coming from: the archive file would be the "container" for all of the read only games files.

Please note that this is not a patch for WHDLoad! In itself this codebase does nothing - it is intended to be taken in from source and then build into the WHDLoad executable, if possible.

To create compatible archive to be processed by this module you need to run either of the following commands in Amiga

For Lha:
```
lha -earz a <archive>.lha <directory>
```

For Zip:
```
zip -r0N <archive>.zip <directory>
```

Replace the `<archive>` and `<directory>` with the target archive name and source directory name respectively

For programming API see container_api.h
