### Janet System Utilities Functions - jsys

#### Purpose

Provide exported definitions of system level functions for multiple operating systems.

Where sensible provide a unified interface to facilitate ease of use, but allow for
direct usage of system level functions for when a user's needs lay outside the bounds
of predetermination.

**NOTE:**
Nothing has been tested at this time.

If there are issues, file a Issue [here](https://github.com/llmII/jsys/issues) or a 
Ticket [here](https://code.amlegion.org/jsys/ticket).

Want another system specific function? File a Ticket/Issue and/or submit a pull request
or email the author a patch. Author may consider implementing a system specific
function on behalf of the submitter of a ticket under a few conditions.

One condition to make note of is the `flock` family of functions. Those won't be
implemented for *nix systems due to the greater capability of `fnctl`.

To sum the conditions up:
1. No additional function that serves the same purpose as an already existing function
   except to replace the former wherein the function provides greater capability except
   where doing so enhances portability to certain systems. 
