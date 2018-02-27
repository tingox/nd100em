CREATE DATABASE nd100em;
USE nd100em;
CREATE TABLE `c` (					/* tracecode table */
	`rid` bigint(21) DEFAULT '0' NOT NULL,		/* run id */
	`i` INT(10) NOT NULL default '0',		/* instruction number */
	`l` INT(10),					/* run level */
	`a` INT(10) NOT NULL default '0',		/* address */
	`d` INT(10) NOT NULL default '0',		/* data */
	`c` VARCHAR(64) NOT NULL default '',		/* code */
	INDEX `id_i` (`i`)
);

CREATE TABLE `r` (					/* traceregs table */
	`rid` bigint(21) DEFAULT '0' NOT NULL,		/* run id */
	`i` INT(10) NOT NULL default '0',		/* instruction number */
	`l` INT(10),					/* run level */
	`r` CHAR(10) NOT NULL default '',		/* register */
	`v` INT(10) NOT NULL default '0',		/* value */
	INDEX `id_i` (`i`)
);

CREATE TABLE `m` (					/* trace memory accesses table */
	`rid` bigint(21) DEFAULT '0' NOT NULL,		/* run id */
	`i` INT(10) NOT NULL default '0',		/* instruction number */
        `t` VARCHAR(64) NOT NULL default '',		/* type */
	`a` INT(10) NOT NULL default '0',		/* address */
	INDEX `id_i` (`i`)
);

CREATE TABLE `e` (					/* table for keeping instruction run by EXR*/
        `rid` bigint(21) DEFAULT '0' NOT NULL,		/* run id */
	`i` INT(10) NOT NULL default '0',		/* instruction number */
        `d` VARCHAR(255) NOT NULL default '',		/* actual instruction run */
	INDEX `id_i` (`i`)
);

CREATE TABLE `o` (					/* table for other extra info */
        `rid` bigint(21) DEFAULT '0' NOT NULL,		/* run id */
	`i` INT(10) NOT NULL default '0',		/* instruction number */
        `d` VARCHAR(255) NOT NULL default '',		/* data */
	INDEX `id_i` (`i`)
);

/*
 Add known addresses we want to be able to skip dumping here,
 this is so we can remove loads of lines containing subroutines for example.
*/
CREATE TABLE `i` (					/* table traceignore */
	`a` INT(10) NOT NULL default '0',		/* address */
        `c` VARCHAR(255) NOT NULL default '',		/* comment */
	`f` INT(10) NOT NULL default '0',		/* flag */
	`x` INT(10) default '0',			/* extra */
	INDEX `id_a` (`a`)
);

CREATE table `s` (					/* steps table for tracing what happens during an istruction */
        `rid` bigint(21) DEFAULT '0' NOT NULL,		/* run id */
	`i` INT(10) NOT NULL default '0',		/* instruction number */
	`s` INT(10) NOT NULL default '0',		/* step number in instruction */
	`w` VARCHAR(32) NOT NULL default '',		/* what we do in this step */
	INDEX `id_i` (`i`)
);	
