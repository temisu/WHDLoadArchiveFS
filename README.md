# WHDLoadArchiveFS - Lha and Zip opening virtual filesystem for WHDLoad

This is extension for WHDLoad utilizing whdvfs-interface. It creates virtual filesystem from Zip and Lha archives thus allows WHDLoad to run images directly from archives without needing to unpack them

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

For programming API see archivefs_api.h

TODO: whdvfs.i license?
