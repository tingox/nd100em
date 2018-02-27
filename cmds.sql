use nd100em;
CREATE TEMPORARY table `t1` (
        `i` INT(10) NOT NULL default '0',
        `a` VARCHAR(256) NOT NULL default '',
        `d` VARCHAR(256) NOT NULL default '',
        `p` VARCHAR(256) NOT NULL default '',
        INDEX `id_i` (`i`)
);
CREATE TEMPORARY table `t2` (
        `i` INT(10) NOT NULL default '0',
        `d` VARCHAR(256) NOT NULL default '',
        INDEX `id_i` (`i`)
);

CREATE TEMPORARY table `t3` (
	`i` INT(10) NOT NULL default '0',
	`d` VARCHAR(256) NOT NULL default '',
	INDEX `id_i` (`i`)
);

CREATE TEMPORARY table `t4` (
	`i` INT(10) NOT NULL default '0',
	`d` VARCHAR(256) NOT NULL default '',
	INDEX `id_i` (`i`)
);

insert into t1 (i,a) select i, group_concat(w order by s asc separator ',') from s where s=0 group by i;
update t1 set d=(select group_concat(w order by s asc separator ',') from s where s.i=t1.i and s<>0 and s<>100 group by i);
update t1 set p=(select group_concat(w order by s asc separator ',') from s where s.i=t1.i and s=100 group by i);

insert into t2 (i,d) select i, concat_ws(', ',a,d,p) from t1;

insert into t3 select i,concat(' ','(',d,')') from e;

insert into t4 select c.i,concat(c.c,ifnull(t3.d,'')) from c left join t3 on c.i = t3.i;

select @a:=1;
select c.i as instr,c.l as level,c.a as addr,t4.d as code ,ifnull(t2.d,'') as steps from c
inner join t4 on c.i = t4.i
left join t2 on c.i = t2.i;


