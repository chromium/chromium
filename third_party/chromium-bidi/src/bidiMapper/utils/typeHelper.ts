export class TypeHelper {
  public static isString(candidate: any): candidate is string {
    return typeof candidate === 'string' || candidate instanceof String;
  }
}
