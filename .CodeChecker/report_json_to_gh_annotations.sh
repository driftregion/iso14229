#!/bin/bash


jq -c '.reports[]' $1 | while read -r report; do
          file_path=$(echo "$report" | jq -r '.file.path')
          line=$(echo "$report" | jq -r '.line')
          col=$(echo "$report" | jq -r '.column')
          message=$(echo "$report" | jq -r '.message')
          severity=$(echo "$report" | jq -r '.severity')
          checker=$(echo "$report" | jq -r '.checker_name')

          # Convert absolute path to something relative to repository root if needed
          # e.g., remove everything up to your repo. This depends on how your paths look.
          # 
          # For instance, if $GITHUB_WORKSPACE is /home/runner/work/myrepo/myrepo,
          # then:
          # file_path="${file_path#"$GITHUB_WORKSPACE/"}"

          if [ -n "$GITHUB_WORKSPACE" ]; then
            workspace_path="$GITHUB_WORKSPACE/"
          else
            workspace_path=$(pwd)/
          fi
          file_path="${file_path#"$workspace_path"}"
          
          # Decide error vs. warning based on severity
          # (Optional) e.g. if severity == "HIGH", treat it as an error, otherwise a warning
          if [ "$severity" = "HIGH" ]; then
            echo "::error file=$file_path,line=$line,col=$col,title=$checker::$message"
          else
            echo "::warning file=$file_path,line=$line,col=$col,title=$checker::$message"
          fi
        done