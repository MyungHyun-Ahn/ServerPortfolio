import argon2 from "argon2";
import type { ResultSetHeader, RowDataPacket } from "mysql2";

import { appEnv } from "../config/env";
import { getMySqlPool } from "../db/mysql";
import type {
  AccountRecord,
  AuthenticatedAccount,
  LoginRequest,
  RegisterRequest,
} from "../models/auth-types";

class ValidationError extends Error {
  public readonly statusCode = 400;
}

class ConflictError extends Error {
  public readonly statusCode = 409;
}

class AuthenticationError extends Error {
  public readonly statusCode = 401;
}

interface AccountRow extends RowDataPacket {
  account_id: number;
  login_id: string;
  password_hash: string;
  nickname: string;
  status: number;
}

function normalizeRequiredString(value: unknown, fieldName: string, maxLength: number): string {
  if (typeof value !== "string") {
    throw new ValidationError(`${fieldName} must be a string.`);
  }

  const normalized = value.trim();
  if (normalized.length === 0) {
    throw new ValidationError(`${fieldName} is required.`);
  }

  if (normalized.length > maxLength) {
    throw new ValidationError(`${fieldName} is too long.`);
  }

  return normalized;
}

function createPasswordHash(password: string): Promise<string> {
  return argon2.hash(password, {
    type: argon2.argon2id,
    memoryCost: appEnv.argon2.memoryCostKiB,
    timeCost: appEnv.argon2.timeCost,
    parallelism: appEnv.argon2.parallelism,
    hashLength: appEnv.argon2.hashLength,
  });
}

function mapAccountRow(row: AccountRow): AccountRecord {
  return {
    userId: row.account_id,
    loginId: row.login_id,
    passwordHash: row.password_hash,
    nickname: row.nickname,
    status: row.status,
  };
}

export class AccountService {
  public async registerAccount(input: RegisterRequest): Promise<AuthenticatedAccount> {
    const loginId = normalizeRequiredString(input.loginId, "loginId", 64);
    const password = normalizeRequiredString(input.password, "password", 128);
    const nickname = normalizeRequiredString(input.nickname, "nickname", 64);

    const passwordHash = await createPasswordHash(password);

    try {
      const [result] = await getMySqlPool().execute<ResultSetHeader>(
        `INSERT INTO accounts (login_id, password_hash, nickname)
         VALUES (:loginId, :passwordHash, :nickname)`,
        {
          loginId,
          passwordHash,
          nickname,
        },
      );

      return {
        userId: Number(result.insertId),
        loginId,
        nickname,
      };
    } catch (error) {
      if (isDuplicateLoginIdError(error)) {
        throw new ConflictError("loginId already exists.");
      }

      throw error;
    }
  }

  public async authenticate(input: LoginRequest): Promise<AuthenticatedAccount> {
    const loginId = normalizeRequiredString(input.loginId, "loginId", 64);
    const password = normalizeRequiredString(input.password, "password", 128);

    const [rows] = await getMySqlPool().execute<AccountRow[]>(
      `SELECT account_id, login_id, password_hash, nickname, status
       FROM accounts
       WHERE login_id = :loginId
       LIMIT 1`,
      { loginId },
    );

    if (rows.length === 0) {
      throw new AuthenticationError("loginId or password is invalid.");
    }

    const account = mapAccountRow(rows[0]);
    if (account.status !== 1) {
      throw new AuthenticationError("account is not active.");
    }

    const isVerified = await argon2.verify(account.passwordHash, password);
    if (!isVerified) {
      throw new AuthenticationError("loginId or password is invalid.");
    }

    return {
      userId: account.userId,
      loginId: account.loginId,
      nickname: account.nickname,
    };
  }
}

function isDuplicateLoginIdError(error: unknown): boolean {
  if (typeof error !== "object" || error === null) {
    return false;
  }

  const mysqlError = error as { code?: string };
  return mysqlError.code === "ER_DUP_ENTRY";
}

export function getErrorStatusCode(error: unknown): number | null {
  if (typeof error !== "object" || error === null) {
    return null;
  }

  const maybeStatusCode = error as { statusCode?: number };
  return typeof maybeStatusCode.statusCode === "number" ? maybeStatusCode.statusCode : null;
}
