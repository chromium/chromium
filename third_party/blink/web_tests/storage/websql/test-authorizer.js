function terminateTest()
{
    if (window.testRunner)
        testRunner.notifyDone();
}

function logAndTerminateTest(message, error)
{
    log(message + ": " + error.message);
    terminateTest();
}

function cleanup(db)
{
    db.transaction(function(tx) {
            tx.executeSql("DROP TABLE IF EXISTS Test;");
            tx.executeSql("DROP INDEX IF EXISTS TestIndex;");
            tx.executeSql("DROP VIEW IF EXISTS TestView;");
            tx.executeSql("DROP TRIGGER IF EXISTS TestTrigger;");
        }, function(error) { logAndTerminateTest("Cleanup failed", error); });
}

function statementSuccessCallback(statementType)
{
    log(statementType + " statement succeeded.");
}

function statementErrorCallback(statementType, error)
{
    log(statementType + " statement failed: " + error.message);
    return false;
}

function executeStatement(tx, statement, operation)
{
    tx.executeSql(statement, [],
                  function(result) { statementSuccessCallback(operation); },
                  function(tx, error) { return statementErrorCallback(operation, error); });
}

function createTableCallback(tx)
{
    executeStatement(tx, "CREATE TABLE Test (Foo int);", "SQLITE_CREATE_TABLE");
}

function createStatementsCallback(tx)
{
    executeStatement(tx, "CREATE INDEX TestIndex ON Test (Foo);", "SQLITE_CREATE_INDEX");

    // Even though the following query should trigger a SQLITE_CREATE_TEMP_INDEX operation
    // (according to http://www.sqlite.org/tempfiles.html), it doesn't, and I'm not aware
    // of any other way to trigger this operation. So we'll skip it for now.
    //executeStatement(tx, "SELECT * FROM Test WHERE Foo IN (1, 2, 3);", "SQLITE_CREATE_TEMP_INDEX");

    executeStatement(tx, "CREATE TEMP TABLE TestTempTable (Foo int);", "SQLITE_CREATE_TEMP_TABLE");
    executeStatement(tx, "CREATE TEMP TRIGGER TestTempTrigger INSERT ON Test BEGIN SELECT COUNT(*) FROM Test; END;", "SQLITE_CREATE_TEMP_TRIGGER");
    executeStatement(tx, "CREATE TEMP VIEW TestTempView AS SELECT COUNT(*) FROM Test;", "SQLITE_CREATE_TEMP_VIEW");
    executeStatement(tx, "CREATE TRIGGER TestTrigger INSERT ON Test BEGIN SELECT COUNT(*) FROM Test; END;", "SQLITE_CREATE_TRIGGER");
    executeStatement(tx, "CREATE VIEW TestView AS SELECT COUNT(*) FROM Test;", "SQLITE_CREATE_VIEW");
    executeStatement(tx, "CREATE VIRTUAL TABLE TestVirtualTable USING MissingModule;", "SQLITE_CREATE_VTABLE");
}

function otherStatementsCallback(tx)
{
    executeStatement(tx, "SELECT COUNT(*) FROM Test;", "SQLITE_READ");
    executeStatement(tx, "SELECT COUNT(*) FROM Test;", "SQLITE_SELECT");
    executeStatement(tx, "DELETE FROM Test;", "SQLITE_DELETE");
    executeStatement(tx, "INSERT INTO Test VALUES (1);", "SQLITE_INSERT");
    executeStatement(tx, "UPDATE Test SET Foo = 2 WHERE Foo = 1;", "SQLITE_UPDATE");
    executeStatement(tx, "PRAGMA cache_size;", "SQLITE_PRAGMA");
    executeStatement(tx, "SELECT * FROM pragma_busy_timeout();", "SQLITE_PRAGMA_FUNCTION");

    executeStatement(tx, "ALTER TABLE Test RENAME TO TestTable;", "SQLITE_ALTER_TABLE");
    // Rename the table back to its original name.
    executeStatement(tx, "ALTER TABLE TestTable RENAME TO Test;", "SQLITE_ALTER_TABLE");

    executeStatement(tx, "ALTER TABLE Test RENAME COLUMN Foo TO Bar;", "SQLITE_ALTER_TABLE");
    // Rename the column back to its original name.
    executeStatement(tx, "ALTER TABLE Test RENAME COLUMN Bar TO Foo;", "SQLITE_ALTER_TABLE");

    executeStatement(tx, "ALTER TABLE Test ADD COLUMN Bla int;", "SQLITE_ADD_COLUMN");
    // Dropping the just added column, this is supposed to fail.
    executeStatement(tx, "ALTER TABLE Test DROP COLUMN Bla;", "SQLITE_DROP_COLUMN");

    executeStatement(tx, "BEGIN TRANSACTION;", "SQLITE_TRANSACTION");
    executeStatement(tx, "ATTACH main AS TestMain;", "SQLITE_ATTACH");
    executeStatement(tx, "DETACH TestMain;", "SQLITE_DETACH");
    executeStatement(tx, "REINDEX;", "SQLITE_REINDEX");
    executeStatement(tx, "ANALYZE;", "SQLITE_ANALYZE");

    // SQLITE_FUNCTION: allowed write mode
    // There is no SQL/Javascript API to add user-defined functions to SQLite,
    // so we cannot test this operation
}

function dropStatementsCallback(tx)
{
    executeStatement(tx, "DROP INDEX TestIndex;", "SQLITE_DROP_INDEX");

    // SQLITE_DROP_TEMP_INDEX: allowed in write mode
    // Not sure how to test this: temp indexes are automatically dropped when
    // the database is closed, but HTML5 doesn't specify a closeDatabase() call.

    executeStatement(tx, "DROP TABLE TestTempTable;", "SQLITE_DROP_TEMP_TABLE");
    executeStatement(tx, "DROP TRIGGER TestTempTrigger;", "SQLITE_DROP_TEMP_TRIGGER");
    executeStatement(tx, "DROP VIEW TestTempView;", "SQLITE_DROP_TEMP_VIEW");
    executeStatement(tx, "DROP TRIGGER TestTrigger;", "SQLITE_DROP_TRIGGER");
    executeStatement(tx, "DROP VIEW TestView;", "SQLITE_DROP_VIEW");

    // SQLITE_DROP_VTABLE: allowed in write mode
    // Not sure how to test this: we cannot create a virtual table because we do not
    // have SQL/Javascript APIs to register a module that implements a virtual table.
    // Therefore, trying to drop a virtual table will always fail (no such table)
    // before even getting to the authorizer.

    executeStatement(tx, "DROP TABLE Test;", "SQLITE_DROP_TABLE");
}

function testReadWriteMode(db)
{
    db.transaction(function(tx) {
            createTableCallback(tx);
            createStatementsCallback(tx);
            otherStatementsCallback(tx);
            dropStatementsCallback(tx);
        },
        function(error) { logAndTerminateTest("Write transaction failed", error); },
        function() { log("Write transaction succeeded."); });
}

function testReadOnlyMode(db)
{
    // Test the 'CREATE TABLE' operation; it should be disallowed
    db.readTransaction(createTableCallback,
        function(error) { logAndTerminateTest("Read 'CREATE TABLE' transaction failed", error); });

    // In order to test all other 'CREATE' operations, we must create the table first
    db.transaction(createTableCallback,
        function(error) { logAndTerminateTest("Write 'CREATE TABLE' transaction failed", error); });
    db.readTransaction(createStatementsCallback,
        function(error) { logAndTerminateTest("Read 'CREATE' transaction failed", error); });

    // In order to test the 'DROP' and 'other' operations, we need to first create the respective entities
    db.transaction(createStatementsCallback,
        function(error) { logAndTerminateTest("Write 'CREATE' transaction failed", error); });
    db.readTransaction(otherStatementsCallback,
        function(error) { logAndTerminateTest("Read 'other' transaction failed", error); });

    // Hack: insert an empty write transaction to guaratee that these transactions are executed sequentially
    db.transaction(function(tx) { });
    db.readTransaction(dropStatementsCallback,
        function(error) { logAndTerminateTest("Read 'DROP' transaction failed", error); },
        function() { log("Read transactions succeeded."); terminateTest(); });
}

function runTest()
{
    var db = openDatabaseWithSuffix("AuthorizerTest", "1.0", "Tests the database authorizer.", 32768);
    cleanup(db);
    testReadWriteMode(db);
    testReadOnlyMode(db);
}
