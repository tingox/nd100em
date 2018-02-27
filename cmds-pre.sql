use nd100em;
select @a:= max(rid)+1 from c;
update c set rid=@a where rid like 0;
update r set rid=@a where rid like 0;
update m set rid=@a where rid like 0;
update o set rid=@a where rid like 0;
update e set rid=@a where rid like 0;
update s set rid=@a where rid like 0;
#remove the code load into memory
delete from m where rid=@a and i=0;

