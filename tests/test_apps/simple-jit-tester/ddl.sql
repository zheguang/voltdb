-- 
CREATE TABLE my_table
(
  i integer
, i_not_null integer NOT NULL
);
--create unique index myidx on my_table(i, i_not_null);
--partition table my_table on column i_not_null;
-- stored procedures
CREATE PROCEDURE FROM CLASS jittester.procedures.Initialize;
CREATE PROCEDURE FROM CLASS jittester.procedures.Select;
