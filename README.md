# File System Checker for xv6

This tool is designed to check the consistency of the xv6 file system image. It thoroughly analyzes the on-disk format and performs checks to verify the integrity of various parts of the file system structure.

## How It Works

The program reads the file system image and checks the consistency against several rules. If it detects any inconsistency, it outputs an error message to the standard error and exits with exit code 1. The following are the checks performed:

1. Inode Validation: Verifies each inode is either unallocated or one of the valid types (T_FILE, T_DIR, T_DEV). Errors out with `ERROR: bad inode` if inconsistencies are found.

2. Block Address Validation: Ensures that each block address used by an in-use inode is valid. Errors include `ERROR: bad direct address in inode for invalid direct blocks` and `ERROR: bad indirect address in inode for invalid indirect blocks`.

3. Root Directory Check: Confirms the existence of the root directory, its inode number as 1, and the parent of the root directory is itself. Errors with `ERROR: root directory does not exist if not met`.

4. Directory Format: Checks if each directory contains . and .. entries, and the . entry points to the directory itself. On failure, prints `ERROR: directory not properly formatted`.

5. Bitmap Consistency: Verifies for in-use inodes, each block address in use is marked in use in the bitmap. If not, prints `ERROR: address used by inode but marked free in bitmap`.

6. Bitmap and Inode Synchronization: Ensures blocks marked in-use in bitmap are actually in-use in an inode or indirect block. If not, prints `ERROR: bitmap marks block in use but it is not in use`.

7. Unique Direct Address: Checks each direct address in use is only used once. If not, prints `ERROR: direct address used more than once`.

8. Unique Indirect Address: Ensures each indirect address in use is only used once. If not, prints `ERROR: indirect address used more than once`.

9. Inode Directory Reference: Confirms all inodes marked in use are referred to in at least one directory. If not, prints `ERROR: inode marked use but not found in a directory`.

10. Inode Reference in Directory: Verifies each inode number referred to in a directory is marked in use. If not, prints `ERROR: inode referred to in directory but marked free`.

11. File Reference Count: Checks that reference counts for regular files match the number of times the file is referred to in directories. If not, prints `ERROR: bad reference count for file`.

12. Directory Link Restriction: Ensures no extra links are allowed for directories; each directory appears only once in another directory. If not, prints `ERROR: directory appears more than once in file system`.

## Compilation (UNIX)

Compile the tool using the following command:

```bash
gcc fcheck.c -o fcheck -Wall -Werror -O
```

Ensure you have the necessary development tools and permissions to compile and run this tool on your system.
