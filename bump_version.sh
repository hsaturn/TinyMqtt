#!/bin/bash
current_version=$(git describe --tags --abbrev=0)
if [ "$1" == "-d" ]; then
  do=0
  shift
else
  do=1
fi
if [ "$1" == "" ]; then
	echo
	echo "Syntax: $0 [-d] {new_version} [commit message]"
	echo
	echo "  -d : dry run, generate json and update properties but do not run git commands"
	echo ""
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

      cp library.json.skeleton library.json
      while ifs= read -r line; do
        name=$(echo "$line" | sed "s/=.*//g")
        value=$(echo "$line" | cut -d= -f 2 | sed 's/"//g')
        echo "  Replacing $name in json"
        if [ "$name" == "depends" ]; then
          depends=$(echo "$value" | sed "s/,/ /g")
          echo "  Depends=$depends"
        fi
        sed -i "s@#$name@$value@g" library.json
      done < library.properties
      deps=""
      for depend in $depends; do
        if [ "$deps" != "" ]; then
          deps="$deps, "
        fi
        deps="$deps'$depend' : '*'"
      done
      sed -i "s@#dependencies@$deps@g" library.json
      sed -i "s/'/\"/g" library.json
      if [ "$do" == "1" ]; then
        echo "Pushing all"
        git tag $1
        git add library.properties
        git add library.json
        git commit -m "Release $1 $2"
        git push
        git push --tags
      fi
		else
			echo "Current version does not match library.property version, aborting"
		fi
	fi
fi
