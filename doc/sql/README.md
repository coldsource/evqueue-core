# Creating evQueue database

Edit the **init.sql** file and change user and password according to your needs.

Use an SQL administrator user to run SQL script :

```
mysql -u root <init.sql
```

Test everything works as expected :

```
mysql -h localhost -u evqueue -p evqueue
```
