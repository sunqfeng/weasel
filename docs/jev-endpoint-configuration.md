# Decision endpoint configuration

The Jev client accepts two optional environment variables:

| Variable | Default | Meaning |
| --- | --- | --- |
| `WEASEL_JEV_ENDPOINT` | Existing upstream HTTPS decision endpoint | Full HTTPS URL for a compatible decision service. |
| `WEASEL_JEV_MODEL` | `jev-latest` | Model identifier included in the decision request. |

The existing `TYPESAFE_API_KEY` supplies the bearer credential. `WEASEL_JEV_ENABLED` and the mandatory application allow-list `WEASEL_JEV_APPS` retain their existing behavior. Restart the server after changing its environment.

`WEASEL_JEV_APPS` is a comma- or semicolon-separated list of executable names (case-insensitive). `*` enables every application, and `!name.exe` excludes one application even when `*` is present, for example `*, !keepass.exe`. A list that contains only exclusions enables nothing.

Custom endpoints must use HTTPS, must not embed a username or password, and must not contain a query or fragment. HTTP redirects are rejected so the bearer credential cannot be forwarded to a different destination. TLS certificate verification remains enabled.

The service must accept the existing `state` and `questions.intent` choice request and return `answers.intent` with a declared choice and valid probabilities. Canonicalized response model names do not affect choice validation.

Transport timeouts are bounded at 1 second for resolution/connection, 1.5 seconds for sending, and 2.5 seconds for receiving. Requests remain asynchronous; these are per-operation bounds rather than an overall deadline. A failed or late request leaves ordinary Rime input behavior in place.

## Optional local diagnostics

`WEASEL_JEV_DIAGNOSTIC_LOG` selects an append-only local diagnostic file. It works independently of release logging macros and records startup readiness, request completion, and selection validation. It is disabled when unset.

`WEASEL_JEV_LOG_DECISIONS=1` additionally records the returned choice, confidence (when supplied), and probability map. `WEASEL_JEV_LOG_CANDIDATES=1` adds candidate text and the raw-input choice to those decision events. Candidate logging can reveal private input: enable it only for deliberate tests, keep logs outside the repository, and disable it afterward. Context text, authorization headers, and credentials are not logged. Existing logs are not retroactively enriched. Probability values are serialized as JSON strings by Boost.PropertyTree.

Keep deployment URLs, model selections, credentials, local launch scripts, and captured request content outside the repository. Prefer process-scoped credentials over permanently storing a bearer token in the user environment. Enable cloud processing only for applications whose input may be sent to the selected service.

## Candidate window indicator

While a candidate menu is shown in an allowed application, the candidate window shows the decision state as a small drawn shape. Nothing is shown when Jev is disabled or the application is not on the allow-list.

| Shape | State |
| --- | --- |
| Spinning ring (panel corner) | A decision request is in flight. |
| Star on a candidate | Jev recommends that candidate with probability ≥ 0.60. |
| Star at the panel corner | Jev recommends keeping the raw input; Space commits it. |
| Hollow dot (panel corner) | The decision was below 0.60; the native order is kept. |
| Cloud with a slash (panel corner) | The request failed or timed out; ordinary Rime behavior applies. |

The recommendation only highlights a candidate. It never reorders candidates, and it does not move the highlight if the user has navigated (arrow keys, paging, mouse hover) since the request was made; the star still marks the recommendation. Space commits the raw input only when the menu is still on the first page and unchanged since the request.

The server never pushes updates. While a request is in flight the text service polls the server about every 90 ms (`WEASEL_IPC_POLL_JEV`) for up to about 4 seconds, and redraws only when the visible state changed. Polling stops on focus loss and deactivation. Older text services ignore the indicator and keep the previous Space-time behavior.
