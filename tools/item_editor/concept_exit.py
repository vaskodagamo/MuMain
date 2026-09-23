"""Exit codes of concepts.py (part of the editor protocol) and which error ends in which code."""

import concept_batch
import concept_cancel
import concept_cost
import concept_key
import concept_library
import concept_lock
import concept_output
import concept_paths
import concept_prompt
import concept_refine
import concept_refs
import concept_select
import openai_images

EXIT_OK = 0
EXIT_ERROR = 1          # anything else: I/O, Blender, sips, a broken template or batch
EXIT_USAGE = 2          # bad arguments, unknown item/model/batch/variant, missing reference render
EXIT_REFUSED = 3        # the estimate is over --max-images / --max-cost
EXIT_FAILED = 4         # the run finished but some requests failed (run --resume retries them)
EXIT_NO_KEY = 5         # neither $OPENAI_API_KEY nor the Keychain item
EXIT_BUSY = 6           # the batch (or the refs folder) is locked by another running process
EXIT_CANCELLED = concept_cancel.EXIT_CANCELLED  # 130: SIGTERM / SIGINT


class UsageError(RuntimeError):
    pass


USAGE_ERRORS = (UsageError, concept_select.SelectionError, concept_refine.RefineError, concept_paths.PathError,
                concept_cost.CostError, concept_batch.BatchError, concept_library.LibraryError)
OTHER_ERRORS = (concept_prompt.PromptError, concept_refs.RefError, concept_output.OutputError,
                openai_images.ApiError, OSError, ValueError)
ERRORS = USAGE_ERRORS + (concept_key.NoKeyError, concept_lock.LockBusy) + OTHER_ERRORS


def exit_code_for(error):
    if isinstance(error, concept_key.NoKeyError):
        return EXIT_NO_KEY
    if isinstance(error, concept_lock.LockBusy):
        return EXIT_BUSY
    if isinstance(error, USAGE_ERRORS):
        return EXIT_USAGE
    return EXIT_ERROR
