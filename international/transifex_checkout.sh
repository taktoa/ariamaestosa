#!/bin/sh
export PATH=$PATH:`pwd`
mkdir transifex
cd transifex
tx init
tx set --auto-remote https://www.transifex.net/projects/p/ariamaestosa/
tx pull --all