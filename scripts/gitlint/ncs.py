# Copyright (c) 2022 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

from gitlint.rules import BoolOption, CommitRule, RuleViolation
import re

class NCSSauceTags(CommitRule):
    """This rule enforces the NCS sauce tag specification.
       https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/dm_code_base.html#oss-repositories-downstream-project-history
    """

    # A rule MUST have a human friendly name
    name = "ncs-sauce-tags"

    # A rule MUST have a *unique* id, we recommend starting with UC (for User-defined Commit-rule).
    id = "UC100"

    options_spec = [BoolOption('enable', False, 'Enable the Sauce Tags Rule')]

    def validate(self, commit):
        self.log.debug(f'NCSSauceTags: enable:{self.options["enable"].value} ' \
                       f'sha:{commit.sha}')

        if not self.options['enable'].value:
            self.log.debug(f'Disabled, skipping')
            return

        m = re.match(r'^(Revert\s+\")?\[nrf (mergeup|fromtree|fromlist|noup)\]\s+',
                     commit.message.title)
        if not m:
            return [RuleViolation(self.id, 'Title does not contain a sauce tag',
                                  line_nr=1)]

        tag = m.group(2)

        if not tag:
            return [RuleViolation(self.id, 'Title does not contain a sauce tag',
                                  line_nr=1)]
        self.log.debug(f'Matched sauce tag {tag}')

        if tag == 'mergeup':
            if not commit.is_merge_commit:
                return [RuleViolation(self.id,
                                      'mergeup used in a non-merge commit')]
            if not re.match(r'^\[nrf mergeup\] Merge upstream up to commit \b([a-f0-9]{40})\b',
                    commit.message.title):
                return [RuleViolation(self.id, 'Invalid mergeup commit title')]
        elif tag == 'fromlist':
            regex = r'^Upstream PR: https://github\.com/.*/pull/\d+'
            matches = re.findall(regex, '\n'.join(commit.message.body), re.MULTILINE)
            if len(matches) == 0:
                return [RuleViolation(self.id,
                        'Missing Upstream PR reference in fromlist commit')]
        elif tag == 'fromtree':
            regex = r'^\(cherry picked from commit \b([a-f0-9]{40})\b\)'
            matches = re.findall(regex, '\n'.join(commit.message.body), re.MULTILINE)
            if len(matches) == 0:
                return [RuleViolation(self.id,
                        'Missing cherry-pick reference in fromtree commit')]
