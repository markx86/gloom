# Database architecture

## Requirements

The application is a multiplayer game with a leaderboard and support for user created content.  
Since it's a multiplayer game, it has to support multiple users.  
To do this, the application
must be able to store the **name** and **password information** of each user and
to authenticate their requests, the application must also be able to track each user's
active **session**.  
To build the leaderboard, the server has to calculate the score for each player.  
To achieve this, the application must store cumulative game statistics for every registered user, such as:
**total kills**, **total deaths**, **total wins**, **total games played**, **kill/death ratio**.  
Since the *K/D ratio* can be derived from the *total kills* and the *total deaths*, we can
avoid storing it in the database and we can offload the calculation to the client.  
However, while the **score** is calculated using other values already present in the
database (such as the number of wins, deaths and kills), the score for each user is also needed
building the leaderboard.  
Since computing the value every time we read from
the database can become expensive when there are a lot of users, it's preferable to
use a bit more memory and store the pre-computed score for each user inside the database.  
Since the users can also create custom maps, the application must also store
information about each map, such as **the map name**, **its level data** and **the map's creator**.  
While a map could be uniquely identified by its name and creator, searching for one would
become very expensive when the table grows big.  
Identifying maps this way also makes it harder for users to share them around, since they would have share both the name of
the map and that of its creator.  
Therefore, it's better to identify maps by a numerical ID,
which is fast to compare (which also means faster to search for) and easier for users to share.

## E-R diagram

![er-diagram](.github/images/er-diagram.svg)

## Logical schema

**users** (<ins>username</ins>, password, salt)  
**sessions** (<ins>session_id</ins>, username, expiration_timestamp)  
**stats** (<ins>username</ins>, wins, kills, deaths, games, score)  
**map** (<ins>map_id</ins>, map_name, creator, map_data)

The referential constraints are:
- **sessions**.username => **users**.name
- **stats**.username => **users**.name
- **map**.creator => **users**.name

There is only one entity constraint, and it is that the pair (**map**.creator, **map**.map_name) must be unique.

## Implemented queries

To see where the queries are used in the code, take a look at [`database.ts`](./src/server/database.ts).

This query is for creating the **users** table.
```pgsql
CREATE TABLE IF NOT EXISTS users (
  username VARCHAR(24) PRIMARY KEY,
  password_hash CHAR(64) NOT NULL,
  salt CHAR(16) NOT NULL
)
```

This query is for creating the **sessions** table.
```pgsql
CREATE TABLE IF NOT EXISTS sessions (
  session_id CHAR(64) PRIMARY KEY,
  username VARCHAR(24) NOT NULL UNIQUE,
  expiration_timestamp INT NOT NULL,
  FOREIGN KEY (username) REFERENCES users(username)
)
```

This query is for creating the **stats** table.  
Note that `score` is defined as a generated *stored* column. A generated column is a column whose value depends on the other columns in the table. By default, generated columns are *virtual*, which means that the value is calculated when the column is read. In this case, the generated column is *stored*, which means that the value is calculated and updated whenever the value of one of the colums changes.
```pgsql
CREATE TABLE IF NOT EXISTS stats (
  username VARCHAR(24) PRIMARY KEY,
  wins INT NOT NULL,
  kills INT NOT NULL,
  deaths INT NOT NULL,
  games INT NOT NULL,
  score INT GENERATED ALWAYS AS ((wins + kills) * 100 / (deaths + 1)) STORED,
  FOREIGN KEY (username) REFERENCES users(username)
)
```
  
This query is for creating the **maps** table.  
The `map_id` is defined with as a `SERIAL`, which is an number that is unique for each row. While `SERIAL` is a simple auto-incrementing number, which makes it easy to predict, this is not an issue for this type of data.  
Note the constraint `UC_map_name_creator`, which enforces the uniques of the tuple (`map_name`, `creator`) in the table.
```pgsql
CREATE TABLE IF NOT EXISTS maps (
  map_id SERIAL PRIMARY KEY,
  map_name VARCHAR(32) NOT NULL,
  creator VARCHAR(24) NOT NULL,
  map_data CHAR(164),
  FOREIGN KEY (creator) REFERENCES users(username),
  CONSTRAINT UC_map_name_creator UNIQUE (map_name, creator)
)
```

This query is for expiring session tokens.  
The PostgreSQL function `now()`, returns the current time in various formats. Since the expiration timestamp is in UNIX time, the query has to select the `epoch`, which is the current UNIX time, from the object returned by `now()`.
```pgsql
DELETE FROM sessions
WHERE expiration_timestamp <= EXTRACT(epoch FROM now())
```

This query is for finding the name of the user that owns a specific session ID.  
The query also ensures that the session ID hasn't expired yet, by checking if the expiration timestamp is in the future.
```pgsql
SELECT username
FROM sessions
WHERE session_id = $1 AND expiration_timestamp > EXTRACT(epoch FROM now())
```

The following query stores a user's credentials.  
This query is used when a user creates a new account.
```pgsql
INSERT INTO users (username, password_hash, salt)
VALUES ($1, $2, $3)
```

This query is used to fetch data to verify a user's password.  
It's used during user authentication.
```pgsql
SELECT password_hash, salt
FROM users
WHERE username = $1
```

This query stores a generated session ID inside the database.
If the user already has a valid session ID, it is overwritten with the new one, invalidating the old one.
```pgsql
INSERT INTO sessions (session_id, username, expiration_timestamp)
VALUES ($1, $2, $3)
ON CONFLICT (username)
DO UPDATE SET session_id = excluded.session_id, expiration_timestamp = excluded.expiration_timestamp
```

This query deletes a session ID from the database.
The query is used when a user requests to be logged out.
```pgsql
DELETE FROM sessions
WHERE session_id = $1
```

This query replaces an existing session ID with a new one.  
The query is used to refesh an existing a session.
```pgsql
UPDATE sessions
SET session_id = $1, expiration_timestamp = $2
WHERE session_id = $3
```

The following query updates a user's cumulative statistics.  
Since the score is a generated column, its value will automatically be updated by PostgreSQL.
```pgsql
INSERT INTO stats (username, wins, kills, deaths, games)
VALUES ($1, $2, $3, $4, 1)
ON CONFLICT (username)
DO UPDATE
SET wins = stats.wins + excluded.wins, kills = stats.kills + excluded.kills, deaths = stats.deaths + excluded.deaths, games = stats.games + 1
```

The following query is used to fetch the leaderboard for a specific user.  
The leaderboard is composed by the top 10 players (sorted by score) and the user that made the request to fetch the leaderboard.  
We include the user's own stats so that they can see where they are in relation to the leaderboard.
```pgsql
SELECT username, wins, kills, deaths, games, score
FROM stats
ORDER BY username = $1, score DESC
LIMIT 11
```

This query finds all maps that where created by a specific user.
```pgsql
SELECT map_id, map_name, map_data
FROM maps
WHERE creator = $1
```

This query deletes a map from the database.  
The server ensures that only the user that created the map can delete it, by passing the username of the user that created the request as the parameter for `creator`.
```pgsql
DELETE FROM maps
WHERE map_id = $1 AND creator = $2
```

This query creates an empty map.  
The ID for each map is automatically generated by PostgreSQL and it's returned by the query.
```pgsql
INSERT INTO maps (map_name, creator)
VALUES ($1, $2)
RETURNING map_id
```

This query edits the level data for a map.
```pgsql
UPDATE maps
SET map_data = $1
WHERE map_id = $2 AND creator = $3
```

The following query fetches information (ID, name and creator) about multiple maps identified by multiple map IDs.  
```pgsql
SELECT map_id, map_name, creator
FROM maps
WHERE map_id = ANY($1)
```

This query fetches the level data for the map specified by map ID, along with the name and creator.  
This query differs from the previous one in that it also returns the level data, which is a very large field (164 bytes), along with the basic info about the map.
```pgsql
SELECT map_name, creator, map_data
FROM maps
WHERE map_id = $1
```
