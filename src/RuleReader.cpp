#include "RuleReader.h"
#include "CookingSystem.h"
#include "App.h"
#include "TomlReader.h"


void gReadRuleFile(StringView inPath)
{
	gApp.Log(R"(Reading Rule file "{}".)", inPath);

	// Parse the toml file.
	toml::parse_result rules_toml = toml::parse_file(inPath);
	if (!rules_toml)
	{
		gApp.LogError("Failed to parse Rule file.");
		gApp.LogError("{}", rules_toml.error());
		gApp.SetInitError(TempString512(R"(Failed to parse Rule file "{}". See log for details.)", inPath).AsStringView());
		return;
	}

	// Initialize a reader on the root table.
	TomlReader reader(rules_toml.table(), gCookingSystem.GetRuleStringPool());

	defer
	{
		// At the end if there were any error, tell the app to not start.
		if (reader.mErrorCount)
			gApp.SetInitError("Failed to parse Rule file. See log for details.");
	};

	// Look for the rules.
	if (!reader.OpenArray("Rule"))
		return;

	while (reader.NextArrayElement())
	{
		// Each rule should be a table.
		if (!reader.OpenTable(""))
			continue;

		defer { reader.CloseTable(); };

		CookingRule& rule = gCookingSystem.AddRule();

		reader.Read("Name", rule.mName);

		if (reader.OpenArray("InputFilters"))
		{
			defer { reader.CloseArray(); };

			while (reader.NextArrayElement())
			{
				if (!reader.OpenTable(""))
					continue;
				defer { reader.CloseTable(); };

				InputFilter& input_filter = rule.mInputFilters.emplace_back();

				TempString512 repo_name;
				if (reader.Read("Repo", repo_name))
				{
					// Try to find the repo from the name.
					FileRepo* repo = gFileSystem.FindRepo(repo_name.AsStringView());
					if (repo == nullptr)
					{
						gApp.LogError(R"(Repo "{}" not found.)", repo_name.AsStringView());
						reader.mErrorCount++;
					}
					else
					{
						input_filter.mRepoIndex = repo->mIndex;
					}
				}

				reader.TryReadArray("Extensions",			input_filter.mExtensions);
				reader.TryReadArray("DirectoryPrefixes",	input_filter.mDirectoryPrefixes);
				reader.TryReadArray("NamePrefixes",		input_filter.mNamePrefixes);
				reader.TryReadArray("NameSuffixes",		input_filter.mNameSuffixes);
			}
		}

		reader.Read        ("CommandLine",		rule.mCommandLine);
		reader.TryRead     ("Priority",			rule.mPriority);
		reader.TryRead     ("Vresion",			rule.mVersion);
		reader.TryRead     ("MatchMoreRules",	rule.mMatchMoreRules);
		reader.TryRead     ("DepFilePath",		rule.mDepFilePath);
		reader.TryReadArray("InputPaths",		rule.mInputPaths);
		reader.TryReadArray("OutputPaths",		rule.mOutputPaths);
	}

	// Validate the rules.
	if (!gCookingSystem.ValidateRules())
		gApp.SetInitError("Rules validation failed. See log for details.");
}
