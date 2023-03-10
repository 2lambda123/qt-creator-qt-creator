# Copyright (c) Microsoft Corporation. All rights reserved.
# SPDX-License-Identifier: MIT

if [[ $options[norcs] = off && -o "login" &&  -f $USER_ZDOTDIR/.zprofile ]]; then
	VSCODE_ZDOTDIR=$ZDOTDIR
	ZDOTDIR=$USER_ZDOTDIR
	. $USER_ZDOTDIR/.zprofile
	ZDOTDIR=$VSCODE_ZDOTDIR

	# Apply any explicit path prefix (see #99878)
	if (( ${+VSCODE_PATH_PREFIX} )); then
		export PATH=$VSCODE_PATH_PREFIX$PATH
	fi
	builtin unset VSCODE_PATH_PREFIX
fi
