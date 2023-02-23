#!/bin/bash
current_version=$(git describe --tags --abbrev=0)
cp library.json.skeleton library.json
while ifs= read -r line; do
  name=$(echo "$line" | sed "s/=.*//g")
  value=$(echo "$line" | cut -d= -f 2 | sed 's/"//g')
  sed -i "s/#$name/$value/g" library.json
done < library.properties

if [ "$1" == "" ]; then
	echo
	echo "Syntax: $0 {new_version}"
	echo
	echo "  Current version: $current_version"
	echo
else
	echo "Current version: ($current_version)"
	echo "New version    : ($1)"
	echo -n "Do you want to proceed ? "
	read a
	if [ "$a" == "y" ]; then
		echo "Doing this..."
		grep $current_version library.properties
		if [ "$?" == "0" ]; then
			sed -i "s/$current_version/$1/" library.properties
			if [ 0 == 1 ]; then
        git tag $1
        git add library.properties
        git add library.json
        git commit -m "Release $1"
        git push
        git push --tags
      else
        echo "No git operation made"
      fi
		else
			echo "Current version does not match library.property version, aborting"
		fi
	fi
fi
