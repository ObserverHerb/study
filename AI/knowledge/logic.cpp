#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <cctype>
#include <format>
#include <vector>

namespace Output
{
	namespace Parentheses
	{
		bool Balanced(const std::string &formula)
		{
			int count=0;
			for (char candidate : formula)
			{
				if (candidate == '(')
				{
					count++;
				}
				else
				{
					if (candidate == ')')
					{
						if (count <= 0) return false;
						count--;
					}
				}
			}
			return count == 0;
		}

		std::string Wrap(const std::string &formula)
		{
			bool isAlpha=true;
			for (char candidate : formula)
			{
				if (!std::isalpha(candidate))
				{
					isAlpha=false;
					break;
				}
			}
			if (formula.empty() || isAlpha || (formula.front() == '(' && formula.back() == ')' && Balanced(formula))) return formula;
			return std::format("({})",formula);
		}
	}
};

// A sentence is a statement about the world;
// it can contain many symbols connected by connectives.
class Sentence
{
public:
	virtual ~Sentence() { }
	virtual std::string Formula() const=0;
	virtual std::unordered_set<Sentence*> Symbols()=0;
	virtual bool Evaluate(const std::unordered_set<Sentence*> &model)=0;
	virtual std::string Print() const=0;
};

// A symbol is a special type of sentence that represents
// a single fact about the world. It's not to be confused
// with the building block for a sentence.
class Symbol : public Sentence
{
public:
	Symbol(const std::string &name): name(name) { }

	bool operator==(const Symbol &other) const
	{
		return name == other.name;
	}

	std::string Print() const override
	{
		return name;
	}

	bool Evaluate(const std::unordered_set<Sentence*> &model) override
	{
		const auto candidate=model.find(this);
		if (candidate == model.end()) throw std::out_of_range("Symbol not found in model");
		//return candidate->Evaluate(model);
		return true;
	}

	std::string Formula() const override
	{
		return name;
	}

	std::unordered_set<Sentence*> Symbols() override
	{
		return {this};
	}

	friend struct std::hash<Symbol>;

protected:
	std::string name;
};

class Connective
{
public:
	virtual ~Connective()=0;
protected:

};

class Not : public Sentence
{
public:
	Not(Sentence *operand) : operand(operand) { }

	bool operator==(const Not &other) const
	{
		return operand == other.operand;
	}

	std::string Print() const override
	{
		return std::format("Not({})",operand->Print());
	}

	bool Evaluate(const std::unordered_set<Sentence*> &model) override
	{
		return !operand->Evaluate(model);
	}

	std::string Formula() const override
	{
		return std::format("¬{}",Output::Parentheses::Wrap(operand->Formula()));
	}

	std::unordered_set<Sentence*> Symbols() override
	{
		return operand->Symbols();
	}

	friend struct std::hash<Not>;

protected:
	Sentence *operand;
};

class And : public Sentence
{
public:

	And(Sentence *left,Sentence *right,std::initializer_list<Sentence*> additional={}) : conjuncts({left,right})
	{
		conjuncts.insert(conjuncts.end(),additional.begin(),additional.end());
	}

	bool operator==(const And &other) const
	{
		return conjuncts == other.conjuncts;
	}

	std::string Print() const override
	{
		std::string result;
		for (auto conjunct : conjuncts) result += conjunct->Print() + ", ";
		result.pop_back(); // TODO: more efficient way to do this?
		return std::format("And({})",result);
	}

	void Add(Sentence *conjunct)
	{
		conjuncts.push_back(conjunct);
	}

	bool Evaluate(const std::unordered_set<Sentence*> &model) override
	{
		for (auto conjunct : conjuncts)
		{
			if (!conjunct->Evaluate(model)) return false;
		}
		return true;
	}

	std::string Formula() const override
	{
		std::string result;
		if (conjuncts.size() == 1) return conjuncts.front()->Formula();
		for (auto conjunct : conjuncts)
		{
			result += Output::Parentheses::Wrap(conjunct->Formula());
			if (conjunct != conjuncts.back()) result += " ∧ ";
		}
		return result;
	}

	std::unordered_set<Sentence*> Symbols() override
	{
		std::unordered_set<Sentence*> result;
		for (auto conjunct : conjuncts) result.merge(conjunct->Symbols());
		return result;
	}

	friend struct std::hash<And>;

protected:
	std::vector<Sentence*> conjuncts;
};

class Or : public Sentence
{
public:
	Or(Sentence *left,Sentence *right,std::initializer_list<Sentence*> additional={}) : disjuncts({left,right})
	{
		disjuncts.insert(disjuncts.end(),additional.begin(),additional.end());
	}

	bool operator==(const Or &other) const
	{
		return disjuncts == other.disjuncts;
	}

	std::string Print() const override
	{
		std::string result;
		for (auto disjunct : disjuncts) result += disjunct->Print() + ", ";
		result.pop_back(); // TODO: more efficient way to do this?
		return std::format("Or({})",result);
	}

	bool Evaluate(const std::unordered_set<Sentence*> &model) override
	{
		for (auto disjunct : disjuncts)
		{
			if (disjunct->Evaluate(model)) return true;
		}
		return false;
	}

	std::string Formula() const override
	{
		std::string result;
		if (disjuncts.size() == 1) return disjuncts.front()->Formula();
		for (auto disjunct : disjuncts)
		{
			result += Output::Parentheses::Wrap(disjunct->Formula());
			if (disjunct != disjuncts.back()) result += " ∨ ";
		}
		return result;
	}

	std::unordered_set<Sentence*> Symbols() override
	{
		std::unordered_set<Sentence*> result;
		for (auto disjunct : disjuncts) result.merge(disjunct->Symbols());
		return result;
	}

protected:
	std::vector<Sentence*> disjuncts;
};

class Implication : public Sentence
{
public:
	Implication(Sentence *antecedent,Sentence *consequent) : antecedent(antecedent), consequent(consequent) { }

	bool operator==(const Implication &other) const
	{
		return antecedent == other.antecedent && consequent == other.consequent;
	}

	std::string Print() const override
	{
		return std::format("Implication({}, {})",antecedent->Print(),consequent->Print());
	}

	bool Evaluate(const std::unordered_set<Sentence*> &model) override
	{
		return !antecedent->Evaluate(model) || consequent->Evaluate(model);
	}

	std::string Formula() const override
	{
		return std::format("{} → {}",Output::Parentheses::Wrap(antecedent->Formula()),Output::Parentheses::Wrap(consequent->Formula()));
	}

	std::unordered_set<Sentence*> Symbols() override
	{
		std::unordered_set<Sentence*> result=antecedent->Symbols();
		result.merge(consequent->Symbols());
		return result;
	}

	friend struct std::hash<Implication>;

protected:
	Sentence *antecedent;
	Sentence *consequent;
};

class Biconditional : public Sentence
{
public:
	Biconditional(Sentence *left,Sentence *right) : left(left), right(right) { }

	bool operator==(const Biconditional &other) const
	{
		return left == other.left && right == other.right;
	}

	std::string Print() const override
	{
		return std::format("Biconditional({}, {})",left->Print(),right->Print());
	}

	bool Evaluate(const std::unordered_set<Sentence*> &model) override
	{
		return left->Evaluate(model) == right->Evaluate(model);
	}

	std::string Formula() const override
	{
		return std::format("{} ↔ {}",Output::Parentheses::Wrap(left->Formula()),Output::Parentheses::Wrap(right->Formula()));
	}

	std::unordered_set<Sentence*> Symbols() override
	{
		std::unordered_set<Sentence*> result=left->Symbols();
		result.merge(right->Symbols());
		return result;
	}

protected:
	Sentence *left;
	Sentence *right;
};

namespace std
{
	template <>
	struct hash<Symbol>
	{
		std::size_t operator()(const Symbol &symbol) const
		{
			return std::hash<std::string>()(symbol.name);
		}
	};

	template <>
	struct hash<Not>
	{
		std::size_t operator()(const Not &candidate) const
		{
			return std::hash<std::string>()("not") ^ std::hash<Not>()(candidate.operand);
		}
	};

	template<>
	struct hash<And>
	{
		std::size_t operator()(const And &candidate) const
		{
			std::size_t hash=std::hash<std::string>()("and");
			for (auto conjunct : candidate.conjuncts)
			{
				hash ^= std::hash<Sentence*>()(conjunct);
			}
			return hash;
		}
	};

	template<>
	struct hash<Implication>
	{
		std::size_t operator()(const Implication &candidate) const
		{
			return std::hash<std::string>()("implies") ^ std::hash<Sentence*>()(candidate.antecedent) ^ std::hash<Sentence*>()(candidate.consequent);
		}
	};
}

int main()
{
	Symbol rain("rain");
	Symbol hagrid("hagrid");
	Symbol dumbledore("dumbledore");

	Sentence *knowledge=new And(
		new Implication(new Not(&rain),&hagrid),
		new Or(&hagrid,&dumbledore),
		{
		new Not(new And(&hagrid,&dumbledore)),
			&dumbledore
		}
	);
	//Sentence *sentence=new And(&rain,&hagrid);

	std::cout << knowledge->Formula() << std::endl;

	return 0;
}