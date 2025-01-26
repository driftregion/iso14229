#!/usr/bin/env python3
import os
import sys
import json
import requests

def chunked(lst, size=50):
    """Yield successive `size`-sized chunks from the list."""
    for i in range(0, len(lst), size):
        yield lst[i:i + size]

def main(report_path):
    # 1. Read environment variables needed for GitHub API
    github_token = os.environ.get("GITHUB_TOKEN")
    if not github_token:
        print("Error: GITHUB_TOKEN not found in environment.")
        sys.exit(1)

    repository = os.environ.get("GITHUB_REPOSITORY")
    if not repository:
        print("Error: GITHUB_REPOSITORY not found in environment.")
        sys.exit(1)

    head_sha = os.environ.get("GITHUB_SHA")
    if not head_sha:
        print("Error: GITHUB_SHA not found in environment.")
        sys.exit(1)

    owner, repo = repository.split("/")
    headers = {
        "Authorization": f"Bearer {github_token}",
        "Accept": "application/vnd.github+json"
    }

    # 2. Read CodeChecker JSON report
    with open(report_path, "r") as f:
        data = json.load(f)

    reports = data.get("reports", [])
    if not reports:
        print("No CodeChecker reports found. Exiting without annotations.")
        # Optionally, you might create a check run that says "No issues found".
        sys.exit(0)

    # 3. Build annotations from each CodeChecker issue
    annotations = []
    highest_severity = "LOW"  # track highest severity seen

    for r in reports:
        file_path = r["file"]["path"]
        line = r.get("line", 1)
        col = r.get("column", 1)
        message = r.get("message", "No message")
        severity = r.get("severity", "LOW")
        checker = r.get("checker_name", "UnknownChecker")

        # Convert absolute path to relative if needed:
        # e.g. if your build paths differ from your repo structure,
        # you might strip out something like GITHUB_WORKSPACE.
        # For example:
        workspace = os.environ.get("GITHUB_WORKSPACE", "")
        if file_path.startswith(workspace):
            file_path = file_path[len(workspace)+1:]

        # Convert severity to annotation_level
        # GitHub recognizes "notice", "warning", or "failure" for annotation_level.
        # We'll treat HIGH as 'failure', otherwise 'warning' or 'notice'.
        if severity.upper() == "HIGH":
            annotation_level = "failure"
            highest_severity = "HIGH"
        else:
            annotation_level = "warning"

        annotations.append({
            "path": file_path,
            "start_line": line,
            "end_line": line,
            # if you want column coverage:
            "start_column": col,
            "end_column": col,
            "annotation_level": annotation_level,
            "message": message,
            "title": checker
        })

    # Decide final conclusion:
    # e.g., if we found any 'HIGH' severity issues, mark "failure".
    # else "success", or "neutral", depending on your preference.
    conclusion = "success"
    if highest_severity == "HIGH":
        conclusion = "failure"

    # 4. Create a check run (status in_progress) to get an ID
    create_url = f"https://api.github.com/repos/{owner}/{repo}/check-runs"
    create_payload = {
        "name": "CodeChecker Analysis",
        "head_sha": head_sha,
        "status": "in_progress"
    }

    resp = requests.post(create_url, headers=headers, json=create_payload)
    if resp.status_code not in (200, 201):
        print("Error creating check run:", resp.text)
        sys.exit(1)

    check_run_id = resp.json()["id"]
    print(f"Created check run with ID={check_run_id}")

    # 5. Update the check run in multiple batches if needed (50 annotations per request limit).
    update_url = f"https://api.github.com/repos/{owner}/{repo}/check-runs/{check_run_id}"

    all_annotations = len(annotations)
    index = 0

    for chunk in chunked(annotations, 50):
        index += len(chunk)

        # Only on the *final chunk* do we set status "completed" with conclusion
        is_last_chunk = (index == all_annotations)

        update_payload = {
            "name": "CodeChecker Analysis",
            "head_sha": head_sha,
            # partial updates can keep "status" as "in_progress" until last chunk
            "status": "in_progress",
            "output": {
                "title": "CodeChecker Results",
                "summary": f"Found {all_annotations} issue(s).",
                "annotations": chunk
            }
        }

        if is_last_chunk:
            update_payload["conclusion"] = conclusion
            update_payload["status"] = "completed"

        resp = requests.patch(update_url, headers=headers, json=update_payload)
        if resp.status_code not in (200, 201):
            print("Error updating check run:", resp.text)
            sys.exit(1)

        print(f"Updated check run with {len(chunk)} annotations (total {index}/{all_annotations}). Last chunk: {is_last_chunk}")
        print(update_payload)

    print("All annotations posted successfully.")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python check_run.py <report.json>")
        sys.exit(1)
    main(sys.argv[1])
