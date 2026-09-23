#!/usr/bin/env python3
"""Run labeled Chinese candidate-ranking cases against the live Jev API."""

from __future__ import annotations

import argparse
import json
import os
import sys
import urllib.request
from pathlib import Path
from typing import Any


API_URL = "https://api.typesafe.ai/v1/systemone"
INSTRUCTIONS = (
    "Choose the candidate that is the most natural continuation of the "
    "committed Chinese context. Prioritize the immediate context and Chinese "
    "grammar over the default candidate order; use the phonetic input to "
    "disambiguate candidates."
)


def is_confident(probability: float, runner_up_probability: float) -> bool:
    return probability >= 0.60 or (
        runner_up_probability >= 0
        and probability >= 0.40
        and probability - runner_up_probability >= 0.10
    )


def ask_jev(api_key: str, case: dict[str, Any], timeout: float) -> dict[str, Any]:
    candidates = case["candidates"]
    payload = {
        "state": {
            "committed_context": case["context"],
            "phonetic_input": case["preedit"],
        },
        "model": "jev-latest",
        "questions": {
            "candidate": {
                "type": "choice",
                "instructions": INSTRUCTIONS,
                "criteria": {f"c{i}": value for i, value in enumerate(candidates)},
            }
        },
    }
    request = urllib.request.Request(
        API_URL,
        data=json.dumps(payload, ensure_ascii=False).encode("utf-8"),
        headers={
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "application/json; charset=utf-8",
            "User-Agent": "Weasel-Jev-Accuracy/0.1",
        },
        method="POST",
    )
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return json.load(response)["answers"]["candidate"]


def evaluate_case(api_key: str, case: dict[str, Any], timeout: float) -> dict[str, Any]:
    answer = ask_jev(api_key, case, timeout)
    choice = answer["choice"]
    candidate_index = int(choice[1:])
    selected = case["candidates"][candidate_index]
    probabilities = answer.get("probabilities", {})
    probability = float(probabilities.get(choice, answer.get("confidence", 0)))
    runner_up = max(
        (float(value) for key, value in probabilities.items() if key != choice),
        default=-1,
    )
    confident = is_confident(probability, runner_up)
    correct = selected == case["expected"]
    return {
        "id": case["id"],
        "expected": case["expected"],
        "selected": selected,
        "correct": correct,
        "confident": confident,
        "effective_correct": correct and confident,
        "probability": probability,
        "runner_up_probability": runner_up,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--cases",
        type=Path,
        default=Path(__file__).with_name("cases.json"),
    )
    parser.add_argument("--output", type=Path)
    parser.add_argument("--timeout", type=float, default=15)
    parser.add_argument("--min-accuracy", type=float, default=0)
    args = parser.parse_args()

    api_key = os.environ.get("TYPESAFE_API_KEY", "").strip()
    if not api_key:
        print("TYPESAFE_API_KEY is not configured.", file=sys.stderr)
        return 2

    cases = json.loads(args.cases.read_text(encoding="utf-8"))
    results = []
    for case in cases:
        try:
            result = evaluate_case(api_key, case, args.timeout)
        except Exception as error:
            result = {"id": case["id"], "error": str(error)}
        results.append(result)
        if "error" in result:
            print(f"ERROR {result['id']}: {result['error']}")
        else:
            mark = "PASS" if result["correct"] else "FAIL"
            applied = "apply" if result["confident"] else "keep-rime"
            print(
                f"{mark} {result['id']}: expected={result['expected']} "
                f"selected={result['selected']} probability={result['probability']:.2f} "
                f"runner_up={result['runner_up_probability']:.2f} {applied}"
            )

    completed = [result for result in results if "error" not in result]
    correct = sum(result["correct"] for result in completed)
    effective_correct = sum(result["effective_correct"] for result in completed)
    summary = {
        "total": len(cases),
        "completed": len(completed),
        "errors": len(cases) - len(completed),
        "top1_correct": correct,
        "top1_accuracy": correct / len(completed) if completed else 0,
        "effective_correct": effective_correct,
        "effective_accuracy": effective_correct / len(completed) if completed else 0,
        "results": results,
    }
    print(json.dumps({key: value for key, value in summary.items() if key != "results"},
                     ensure_ascii=False, indent=2))
    if args.output:
        args.output.write_text(
            json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
    return 0 if summary["top1_accuracy"] >= args.min_accuracy else 1


if __name__ == "__main__":
    raise SystemExit(main())
