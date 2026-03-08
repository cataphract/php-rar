This file contains the procedure to update to a new version of unrar.

## Obtaining new versions of unrar

There is a separate branch, `unrar` with the unaltered source code of unrar.
Each commit is a new version of `unrar`. They are committed sequentially by
version number, as `unrar` doesn't generally have minor updates for older
versions.

The commits have messages like "Added unrar 5.9.4".

This branch is then merged to master, with the subtree strategy.

To update to new versions of unrar, follow this procedure:

1. Check the latest version of unrar from https://www.rarlab.com/rar_add.htm.
   Look for the link with the name "UnRAR source". Not the url pattern of the
   link, as this will be needed for step 3.
2. Setup a worktree with the unrar branch. Check the latest commit. This will
   indicate what the last unrar version that was handled.
3. Attempt to download the next patch version. If you get a 404, attempt the
   next minor version, otherwise the next major version.
4. Extract the contents into the `unrar` directory of the repository,
   completely replacing the previous contents.
5. Commit with the correct message.
6. If you handled the latest version, you are done. Otherwise, go to step 3.

## Updating the extension

After the unrar branch has the latest version of unrar, it's time to merge the
unrar branch.

1. First, use `git merge-base HEAD unrar` to determine the last unrar version
   that was merged.
2. Determine how the unrar extension was modified in the `master` branch.
   Compare the contents of the `unrar` subdirectory in `master` branch to the
   original content in the `unrar` branch. This will inform your conflict
   resolution afterwards. Note in a file what these changes are, so you can
   refer to them later.
3. Upgrade to next minor (not patch, not major) version that hasn't been merged
   yet.
4. Resolve any conflicts that may have arisen, preserving the functionality that
   was added to the unrar library and the associated extension functionality.
5. Test. Inspect the `Justfile`. Run the tests for 7.0 and the latest supported
   PHP version (debug and release-zts).
7. Continue with the next minor version on step 3, until we're synced with the
   `unrar` branch.

<!-- vim: set tw=80: -->
