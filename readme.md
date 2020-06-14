# cloudCryptFS

FUSE filesystem that stores it's data in encrypted blocks of equal size.
The idea is that it would allow syncing the blocks to cloud storage, without reveiling your file contents or metadata to the cloud provider.

## Possible uses
- Backup your private stuff to a cloud provider without the provider being able to access it
- Work with git repository's in cloud storage

## Features so far:
- Deduplication
- Journaling
- Basic filesystem features
- Links
- Encryption
- Windows support (dokany)
- Docker image with rsync, to allow you to send al sorts of files to a backup space


## Features to do:
- Advanced filesystem features such as locking/xattr
- Getting key file/password from a password safe. Such as keepass2 or system native password stores.
- IV's should be stored with the the metadata. NOT with the bucket... this would ensure consistent state.
- ~~password and/of key input + changing~~
- ~~Journalling filesystem, commit changes 1st to a journal, evict from journal as soon as it is executed.~~
- ~~Migrate to binary record keeping for tree nodes.~~


## Ideas:

## Bugs:

- ~~many locking related issues, might be good to lock the complexobject system.~~

## Weakness:

- ~~All metadata is protected by the 1st stage key, wich is derived from the password with argon2~~
   ~~Should implement key-file, so that all keys are a combination of a key-file and a password.~~

