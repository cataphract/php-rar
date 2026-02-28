This file contains the procedure to update to a new version of unrar.

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

<!-- vim: set tw=80 -->
