# Pervasive
Utility for collecting the list of pervasive resources from the HTTP Archive dataset.

# Process

The rules for what URLs qualify as "pervasive" for the purpose of using a shared cache are laid out in the [Cache sharing for extremely-pervasive resources](https://docs.google.com/document/d/1xaoF9iSOojrlPrHZaKIJMK4iRZKA3AD6pQvbSy4ueUQ/edit?usp=sharing) document.

The script automates the filtering of candidate URLs from the HTTP Archive dataset, collecting data from the last six months of crawls, filters the URLs based on the restrictions and automates creating patterns for the resulting resources.

## Step 1 - Query for candidate URLs

This is the query that is run for each of the last six months of data to produce an initial dataset to filter from:

```sql
#standardSQL
SELECT
    url,
    ANY_VALUE(dest) as dest,
    ANY_VALUE(size) as size,
    ANY_VALUE(request_headers) as request_headers,
    ANY_VALUE(response_headers) as response_headers,
    body_hash,
    COUNT(*) as num
FROM (
    SELECT
        url,
        PARSE_NUMERIC(JSON_VALUE(payload, "$._objectSize")) as size,
        JSON_VALUE(payload, "$._body_hash") as body_hash,
        req_h.value as dest,
        request_headers,
        response_headers
    FROM
        `httparchive.crawl.requests`,
        UNNEST (request_headers) as req_h,
        UNNEST (response_headers) as resp_h
    WHERE
        date = "{year}-{month}-01" AND
        JSON_VALUE(payload, "$._body_hash") IS NOT NULL AND
        lower(resp_h.name) = "cache-control" AND
        lower(resp_h.value) LIKE "%public%" AND
        lower(req_h.name) = "sec-fetch-dest" AND
        (lower(req_h.value) = "script" OR lower(req_h.value) = "style" OR lower(req_h.value) = "empty") AND
        PARSE_NUMERIC(JSON_VALUE(payload, "$._responseCode")) = 200 AND
        PARSE_NUMERIC(JSON_VALUE(payload, "$._objectSize")) > 1000
) Hashes
GROUP BY url, body_hash
HAVING COUNT(*) > 20000
ORDER BY num DESC
```

## Step 2 - Filter candidates based on requirements

* Exclude any requests with an `empty` destination that do not include a `use-as-dictionary` response header.
* Exclude any requests that include query parameters in the URL.
* Exclude any requests where the response does not include `public` in the `cache-control` response header.
* Exclude any requests where the response includes a `set-cookie` response header.

## Step 3 - Identify static URLs

## Step 4 - Identify patterns