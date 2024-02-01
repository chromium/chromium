# Process for rolling new versions of unrar

Unrar is released on the [official
site](https://www.rarlab.com/rar_add.htm), but Chromium uses a [GitHub
mirror](https://github.com/aawc/unrar) for tracking and fuzzing. This
means that at a high level, rolling a new version of unrar has the
following steps:
- Update the GitHub repo
- Wait a week for fuzzing on the new version
- Update the chromium repo

# Updating the GitHub repo

You'll need a GitHub account to do this. Create a fork of the [GitHub
repo](https://github.com/aawc/unrar), and clone that fork to your
local machine (replacing `$username` with your GitHub username):

```
git clone https://github.com/$username/unrar.git
```

You will likely need to configure this repo to use your GitHub
account's name, email, and to sign commits. Follow
[these](https://docs.github.com/en/authentication/managing-commit-signature-verification/generating-a-new-gpg-key#generating-a-gpg-key)
steps to create a GPG key for your account, then run the following:

```
git config user.name "FIRST_NAME LAST_NAME"
git config user.email "email-for-github@domain"
git config commit.gpgsign true
```

Then you can download the newest release from the [official
site](https://www.rarlab.com/rar_add.htm). Overwrite the contents of
your repo with the contents of that archive. Then commit the
changes. Commit messages have historically used a format like:

```
v6.2.12: Extracted from https://www.rarlab.com/rar/unrarsrc-6.2.12.tar.gz
```

Pushing your commit to your GitHub fork requires a personal access
token. Follow the steps
[here](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/managing-your-personal-access-tokens)
to create one, then:

```
git push
```

Create a pull request from your GitHub fork and notify a
[//third_party/unrar/OWNER](https://source.chromium.org/chromium/chromium/src/+/main:third_party/unrar/OWNERS)
about the change.
