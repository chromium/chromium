#!/bin/bash

set -e

# Parse arguments
DRY_RUN=false
while [[ $# -gt 0 ]]; do
    case $1 in
        --dry-run|-d)
            DRY_RUN=true
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Cherry-pick commits from PRs labeled 'stable-nominated' to current branch"
            echo ""
            echo "Options:"
            echo "  -d, --dry-run    Show what would be done without making changes"
            echo "  -h, --help       Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

if [ "$DRY_RUN" = true ]; then
    echo "[DRY RUN MODE - No changes will be made]"
    echo ""
fi

current_branch=$(git branch --show-current)
echo "Current branch: $current_branch"
echo "Fetching PRs with 'stable-nominated' label..."
echo ""

# Get PRs with stable-nominated label that are merged
# Sort by merge date (oldest first) to preserve merge order and avoid conflicts
# Format: PR number, title, merge commit SHA
prs=$(gh pr list --state merged --label stable-nominated --json number,title,mergeCommit,mergedAt --jq 'sort_by(.mergedAt) | .[] | "\(.number)|\(.title)|\(.mergeCommit.oid)"')

if [ -z "$prs" ]; then
    echo "No PRs found with 'stable-nominated' label."
    exit 0
fi

# Arrays to track results
declare -a successful
declare -a failed
declare -a skipped

echo "Found PRs to cherry-pick:"
echo ""

# Process each PR
while IFS='|' read -r pr_number title commit_sha; do
    echo "----------------------------------------"
    echo "PR #${pr_number}: ${title}"
    echo "Commit: ${commit_sha}"

    # Check if commit already exists in current branch
    if git branch --contains "$commit_sha" 2>/dev/null | grep -q "^\*"; then
        echo "⏭  Already cherry-picked, skipping"
        skipped+=("PR #${pr_number}: ${title}")
        echo ""
        continue
    fi

    # Cherry-pick with -xe flags as specified
    if [ "$DRY_RUN" = true ]; then
        echo "Would cherry-pick with: git cherry-pick -xe $commit_sha"
        echo "Would add backport note: (backport https://github.com/rust-lang/libc/pull/$pr_number)"
        successful+=("PR #${pr_number}: ${title} (${commit_sha:0:8})")
    else
        if git cherry-pick -xe "$commit_sha" 2>&1; then
            # Add backport note before the cherry-pick note as per CONTRIBUTING.md
            current_msg=$(git log -1 --format=%B)
            backport_line="(backport https://github.com/rust-lang/libc/pull/$pr_number)"

            # Insert backport line before "(cherry picked from commit" line
            new_msg=$(echo "$current_msg" | sed "/^(cherry picked from commit/i\\
$backport_line\\
")

            # Amend the commit with the new message
            git commit --amend -m "$new_msg"

            echo "✓ Successfully cherry-picked with backport note"
            successful+=("PR #${pr_number}: ${title} (${commit_sha:0:8})")
        else
            echo "✗ Failed to cherry-pick"
            failed+=("PR #${pr_number}: ${title} (${commit_sha:0:8})")
            # Abort the failed cherry-pick
            git cherry-pick --abort 2>/dev/null || true
        fi
    fi
    echo ""
done <<< "$prs"

# Print summary
echo "========================================"
if [ "$DRY_RUN" = true ]; then
    echo "SUMMARY (DRY RUN)"
else
    echo "SUMMARY"
fi
echo "========================================"
echo ""

if [ ${#successful[@]} -gt 0 ]; then
    if [ "$DRY_RUN" = true ]; then
        echo "Would cherry-pick (${#successful[@]}):"
    else
        echo "Successfully cherry-picked (${#successful[@]}):"
    fi
    for item in "${successful[@]}"; do
        echo "  ✓ $item"
    done
    echo ""
fi

if [ ${#skipped[@]} -gt 0 ]; then
    echo "Skipped (${#skipped[@]}):"
    for item in "${skipped[@]}"; do
        echo "  ⏭  $item"
    done
    echo ""
fi

if [ ${#failed[@]} -gt 0 ]; then
    echo "Failed (${#failed[@]}):"
    for item in "${failed[@]}"; do
        echo "  ✗ $item"
    done
    echo ""
    if [ "$DRY_RUN" = false ]; then
        echo "Please resolve conflicts manually and re-run if needed."
    fi
    exit 1
fi

if [ "$DRY_RUN" = true ]; then
    echo "Dry run complete! Run without --dry-run to apply changes."
else
    echo "All done!"
fi
