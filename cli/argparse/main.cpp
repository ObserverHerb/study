#include <boost/program_options.hpp>
#include <filesystem>
#include <iostream>

int main(int argumentCount,char *arguments[])
{
	boost::program_options::options_description argumentsDescription("Options");
	argumentsDescription.add_options()
		("help,h","make noise")
		("filename,f",boost::program_options::value<std::string>(),"path to file")
	;

	boost::program_options::variables_map options;
	boost::program_options::store(boost::program_options::parse_command_line(argumentCount,arguments,argumentsDescription),options);
	boost::program_options::notify(options);

	if (auto option=options.find("help"); option != options.end())
	{
		std::cout << argumentsDescription << "\n";
	}
	else
	{
		if (auto filenameOption=options.find("filename"); filenameOption != options.end())
		{
			auto filename=filenameOption->second.as<std::string>();
			if (std::filesystem::exists(std::filesystem::path(filename)))
				std::cout << "File exists!" << "\n";
		}
	}

	return 0;
}
