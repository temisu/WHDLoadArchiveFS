# WHDLoadArchiveFS - Lha and Zip opening virtual filesystem for WHDLoad

## Introduction

This is extension for WHDLoad utilizing whdvfs-interface. It creates virtual filesystem from Zip and Lha archives thus allows WHDLoad to run images directly from archives without needing to unpack them

In order to use this extension, you must have WHDLoad version of 18.7.6188 at minimum

## Installation

Download (or compile) the WHDLoad.VFS and copy it into the installation directory of WHDLoad (which is usually C:)

## Supported formats

This extension supports following formats
* Lha archives with Level0, Level1 and Level2 headers
* Lha compression methods LH4, LH5, LH6
* Zip archives that are RFC-compliant and not zip64
* Zip compression method deflate
* Amiga specific extensions to both Lha and Zip (for example file attributes, file notes)

Please note that by default only archives that are created by Amiga are supported!

Please note that using compression will impact loading performance hugely. Also it requires more memory

## Example (Secret of Monkey Island)

As a simple example lets modify existing installation of Monkey Island to use the archive loading.

1. Open shell
1. Run `cd <game_dir>/data`
1. Run `zip -r0N /data.zip #?`
1. Close shell
1. Edit SecretOfMonkeyIsland.info. Add ToolType `Data=data.zip`

After this, the data is loaded from an zip file you created. Please note that:
* This game does not benefit from archive loading particularly. (there is no big amount of small files). This is just an example
* You must also set SavePath tooltype in order to save games. The archive is read only.


## Compression examples

For Lha (uncompressed):
```
lha -earz a <archive>.lha <directory>
```

For Lha (compressed):
```
lha -earz a <archive>.lha <directory>
```

For Zip (uncompressed):
```
zip -r0N <archive>.zip <directory>
```

For Zip (compressed):
```
zip -rN <archive>.zip <directory>
```

Replace the `<archive>` and `<directory>` with the target archive name and source directory name respectively


## Feedback

Please use the github tracker for bugs, feature requests. PRs are also welcome.

For any other feedback use tz at iki dot fi


## Known issues

* Some rarely used compression features are not tested yet, thus they are probably broken
* Currently the decompression performance is luke warm at best. This is ongoing effort...
* There are some missing test cases
* Lha LH1 is not supported and it is questionable if there is value in supporting it
