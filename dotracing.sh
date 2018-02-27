#!/bin/sh
cat cmds-prepre.sql | mysql
cat tracefile.log | perl -e 'while(<>){s/^#/INSERT INTO /; s/#v#/VALUES/; print;}' | mysql
cat cmds-pre.sql | mysql
cat cmds.sql | mysql > hubba.slask
