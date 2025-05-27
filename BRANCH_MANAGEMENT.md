# Branch Management Strategy: Waterfall Merges with Mid-Chain Amendments

## Overview

This document describes the branch management strategy for maintaining a clean waterfall dependency chain where changes can be efficiently propagated through multiple dependent branches while preserving single-commit pull requests.

## Branch Structure

Our dependency chain follows this waterfall pattern:
```
1_fix/cmake-arm-cross-platform
    ↓
2_feat/concurrent-processing  
    ↓
3_feat/scarletbook_verify
    ↓
4_feat/improved-filename-hints
    ↓
integration_v4 (test/integration branch)
```

Each branch should maintain a **single clean commit** representing the final state of that feature/fix for clean pull requests.

## Core Principles

1. **Single Commit Per Branch**: Each feature/fix branch contains exactly one commit with all changes
2. **Waterfall Propagation**: Changes flow downstream through the dependency chain
3. **Clean Integration**: integration_v4 reflects the combined state of all features
4. **Efficient Operations**: Combine git commands to minimize operations and preserve context

## Making Changes to Mid-Chain Branches

### Scenario: Edit in 2_feat/concurrent-processing

When making changes to a branch that has downstream dependencies:

#### Step 1: Make the change in the target branch
```bash
# Switch to the branch where the change belongs
git checkout 2_feat/concurrent-processing

# Make your changes (edit files)
# ...

# Amend the existing commit to maintain single-commit principle
git add <modified-files> && git commit --amend --no-edit
```

#### Step 2: Propagate through the waterfall (efficient batch operations)
```bash
# Rebase each downstream branch in sequence
git checkout 3_feat/scarletbook_verify && git rebase 2_feat/concurrent-processing &&
git checkout 4_feat/improved-filename-hints && git rebase 3_feat/scarletbook_verify &&
git checkout integration_v4 && git merge 4_feat/improved-filename-hints
```

#### Step 3: Handle conflicts efficiently
When conflicts occur during rebase:
```bash
# Resolve conflicts in editor, then continue in one command
git add <conflict-files> && git rebase --continue
```

For merge conflicts in integration_v4:
```bash
# Resolve conflicts, then complete merge
git add <conflict-files> && git commit
```

#### Step 4: Push all updated branches (parallel operation)
```bash
# Push all branches that were updated
git push --force-with-lease origin 2_feat/concurrent-processing &&
git push --force-with-lease origin 3_feat/scarletbook_verify &&
git push --force-with-lease origin 4_feat/improved-filename-hints &&
git push --force-with-lease origin integration_v4
```

## Conflict Resolution Strategies

### Common Conflict Patterns

1. **Same-line modifications**: Choose the newer/intended version
2. **Feature additions**: Usually keep both features unless they conflict
3. **Terminal output changes**: Prefer newer formatting (e.g., ANSI vs trailing spaces)

### Resolution Shortcuts
```bash
# For simple conflicts, resolve and continue in one command
git add . && git rebase --continue

# For merge conflicts, resolve and commit
git add . && git commit --no-edit
```

## Efficiency Tips

### Combine Git Operations
```bash
# Instead of separate commands:
git add file.c
git commit --amend --no-edit

# Use:
git add file.c && git commit --amend --no-edit
```

### Batch Branch Operations
```bash
# Switch and rebase in one command
git checkout branch-name && git rebase base-branch

# Chain multiple operations
git checkout branch1 && git rebase base &&
git checkout branch2 && git rebase branch1
```

### Conflict Handling
```bash
# When you know the resolution pattern, batch the fix
git add <files> && git rebase --continue
git add <files> && git commit  # for merges
```

## Troubleshooting Common Issues

### "Outdated content" in downstream branches
This happens when branches haven't been rebased after upstream changes. Solution:
```bash
# Force update the entire chain
git checkout 2_feat/concurrent-processing && 
git checkout 3_feat/scarletbook_verify && git rebase 2_feat/concurrent-processing &&
git checkout 4_feat/improved-filename-hints && git rebase 3_feat/scarletbook_verify &&
git checkout integration_v4 && git reset --hard HEAD~1 && git merge 4_feat/improved-filename-hints
```

### Commits appearing in wrong branch
If a commit appears in integration_v4 instead of its proper feature branch:
```bash
# Reset integration_v4 and apply change to correct branch
git checkout integration_v4 && git reset --hard HEAD~1
git checkout correct-branch && git add <files> && git commit --amend --no-edit
# Then re-run the waterfall propagation
```

### Multiple commits in feature branch
To squash multiple commits back to single commit:
```bash
git checkout feature-branch
git reset --soft HEAD~n  # where n is number of commits to squash
git commit --amend --no-edit
```

## Quick Reference Commands

### Start new change in mid-chain branch:
```bash
git checkout target-branch && 
# make changes, then:
git add . && git commit --amend --no-edit
```

### Propagate through waterfall:
```bash
git checkout 3_feat/scarletbook_verify && git rebase 2_feat/concurrent-processing &&
git checkout 4_feat/improved-filename-hints && git rebase 3_feat/scarletbook_verify &&
git checkout integration_v4 && git merge 4_feat/improved-filename-hints
```

### Push all updates:
```bash
git push --force-with-lease origin 2_feat/concurrent-processing 3_feat/scarletbook_verify 4_feat/improved-filename-hints integration_v4
```

## Best Practices

1. **Always amend commits** in feature branches to maintain single-commit principle
2. **Combine git commands** with `&&` to reduce context switching
3. **Use `--force-with-lease`** instead of `--force` for safer force pushes
4. **Resolve conflicts immediately** rather than staging partial fixes
5. **Test in integration_v4** before pushing to ensure the chain works
6. **Keep commit messages descriptive** since they represent the entire feature

## Notes

- This strategy prioritizes clean git history and efficient operations
- Each feature branch represents a complete, reviewable unit of work
- integration_v4 serves as the testing ground for the combined feature set
- The waterfall approach ensures all features work together before merging to main