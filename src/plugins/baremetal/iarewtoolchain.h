// Copyright (C) 2019 Denis Shienkov <denis.shienkov@gmail.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <projectexplorer/toolchain.h>

namespace BareMetal::Internal {

class IarToolChainFactory final : public ProjectExplorer::ToolChainFactory
{
public:
    IarToolChainFactory();

    ProjectExplorer::Toolchains autoDetect(
            const ProjectExplorer::ToolchainDetector &detector) const final;
    ProjectExplorer::Toolchains detectForImport(
            const ProjectExplorer::ToolChainDescription &tcd) const final;

private:
    ProjectExplorer::Toolchains autoDetectToolchains(const Candidates &candidates,
            const ProjectExplorer::Toolchains &alreadyKnown) const;
    ProjectExplorer::Toolchains autoDetectToolchain(
            const Candidate &candidate, Utils::Id languageId) const;
};

} // BareMetal::Internal
