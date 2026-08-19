# XMLBuilder

Command-line tool in C that reads an XML file, dynamically generates an SQL schema (tables + relations) from its structure, then executes that script against a local SQLite database.

## Details

- Every XML element with children becomes an SQL table (`id_<table>` as primary key, XML attributes as `varchar(42)` columns).
- A child element that itself has children (relation) becomes a join table `<parent_table>_<child_table>` with foreign keys.
- A child element that appears more than once under the same parent (with or without attributes) is also promoted to its own table, linked to the parent via a join table — each occurrence becomes a row, with no data loss and no column conflicts.
- The content of each element is inserted row by row into the corresponding tables.
- Requirements : GCC, `libxml2` and `sqlite3` (with their development headers), `pkg-config`

## Usage

```sh
make
make test
./xmlbuilder <file.xml> [database.db] [output.sql]
```

- `file.xml`: path to the XML file to process (required).
- `database.db`: name of the SQLite database to create/update (default: `my_database.db`).
- `output.sql`: generated SQL file (default: `output.sql`).

Example with the file provided in `tests/fixtures/`:

```sh
./xmlbuilder tests/fixtures/example.xml
```

## Limitations

- Fixed sizes on the C side: 100 tables and 100 relations maximum (`MAX_TABLES`, `MAX_RELATIONS` in [include/xmlbuilder.h](include/xmlbuilder.h)), table/attribute names truncated to 256 characters, column content truncated to 512 characters. Beyond that, behavior is not guaranteed.
- The test suite (`make test`) covers the main scenarios but is intentionally minimal (no fuzzing or malformed-XML cases).
